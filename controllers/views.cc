#include <string>
#include <unordered_map>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <memory>
#include <exception>
#include <functional>
#include <random>
#include <fmt/core.h>
#include <drogon/drogon.h>
#include "../config.h"
#include "views.h"

#define authenticate(cb) cb(HttpResponse::newRedirectionResponse(OAUTH_URL)); return
#define toStartPage(cb) cb(HttpResponse::newRedirectionResponse("/")); return
#define voteID(uid, bid) std::to_string(uid)+'-'+std::to_string(bid)
#define cantVote(vid, ...) {auto _cantvoteres = last_votes.find(vid); auto now = trantor::Date::date(); if (_cantvoteres != last_votes.end() and now < _cantvoteres->second.after(43200)) __VA_ARGS__}


struct WebhookEvent {
    std::string title;
    std::optional<std::string> action, reason;
    const Bot &bot;
};

void webhookSend(const WebhookEvent& event, const HttpRequestPtr &req) {
    auto discordapi = HttpClient::newHttpClient("https://discord.com");
    // Build request
    auto apiReq = HttpRequest::newHttpRequest();
    apiReq->setPath("/api/webhooks/" LOG_WEBHOOK);
    apiReq->setContentTypeCode(ContentType::CT_APPLICATION_JSON);
    apiReq->setMethod(HttpMethod::Post);
    // Build embed
    Json::Value data;
    {
        auto& embed = ((data["embeds"] = Json::arrayValue).append(Json::objectValue));
        embed["title"] = event.title;
        embed["color"] = 0xff9933;
        auto& embed_fields = (embed["fields"] = Json::arrayValue);
        std::initializer_list<std::tuple<std::string, std::optional<std::string>>>
                raw_fields = {{"Bot Name", event.bot.name}, {"Bot Owner", event.bot.owner}, {"Link", event.bot.get_link(req)}, {"Action", event.action}, {"Reason", event.reason}};
        for (const auto& [name, value] : raw_fields) {
            if (!value.has_value()) continue;
            Json::Value field;
            field["name"] = name;
            field["value"] = value.value();
            embed_fields.append(std::move(field));
        }
    }
    apiReq->setBody(Json::writeString(Json::StreamWriterBuilder(), data));
    // Send request
    discordapi->sendRequest(apiReq);
}

Bot deserializeBot(orm::Row row) {
    Bot thisbot;
#   define sf(f) thisbot.f = row[#f]
    sf(name).as<std::string>();
    sf(short_description).as<std::string>();
    sf(long_description).as<std::string>();
    sf(avatar_url).as<std::string>();
    sf(owner).as<std::string>();
    sf(support_server).as<std::string>();
    sf(prefix).as<std::string>();
    sf(votes).as<int64_t>();
    sf(approved).as<bool>();
    thisbot.owner_id = std::stoul(row["owner_id"].as<std::string>());
    thisbot.app_id = std::stoul(row["app_id"].as<std::string>());
    return thisbot;
}

std::string dbEsc(const std::string& src) {
    std::ostringstream fres;
    for (const auto &character : src) {
        if (character == '\'') {
            fres << "''";
        } else {
            fres << character;
        }
    }
    return fres.str();
}

auto errPage(const std::exception& e) {
    HttpViewData data;
    data.insert("message", e.what());

    auto resp = HttpResponse::newHttpViewResponse("exception.csp", data);
    resp->setStatusCode(HttpStatusCode::k500InternalServerError);
    return resp;
}

SessionDataPtr getSessionData(SessionPtr session) {
    return session->getOptional<SessionDataPtr>("data").value_or(nullptr);
}
void setSessionData(SessionPtr session, SessionDataPtr data) {
    session->insert("data", data);
}
void resetSessionData(SessionPtr session) {
    session->erase("data");
}

HttpViewData HttpViewDataPrep(SessionDataPtr sessionData) {
    HttpViewData fres;
    fres.insert("sessionData", sessionData);
    return fres;
}
HttpViewData HttpViewDataPrep(SessionPtr session) {
    return HttpViewDataPrep(getSessionData(session));
}
HttpViewData HttpViewDataPrep(const HttpRequestPtr& request) {
    return HttpViewDataPrep(request->session());
}

void getUser(uint64_t user_id, std::function<void (const Json::Value&)> callback) {
    auto discordapi = HttpClient::newHttpClient("https://discord.com");
    auto req = HttpRequest::newHttpRequest();
    req->setPath("/api/v8/users/"+std::to_string(user_id));
    req->setMethod(HttpMethod::Get);
    req->addHeader("Authorization", "Bot " BOT_TOKEN);
    discordapi->sendRequest(req, [callback, user_id] (ReqResult, const HttpResponsePtr &response) {
        if (response->getStatusCode() == HttpStatusCode::k200OK) {
            auto &json = *response->getJsonObject().get();
            auto avatar_hash = json["avatar"].asString();
            if (avatar_hash.empty()) {
                json["avatar_url"] = "https://cdn.discordapp.com/embed/avatars/"+std::to_string(std::stoi(json["discriminator"].asString()) % 5)+".png";
            } else {
                json["avatar_url"] = "https://cdn.discordapp.com/avatars/"+std::to_string(user_id)+"/"+avatar_hash+".png";
            }
            callback(json);
        } else {
            callback({});
        }
    });
}

auto dbErr = [](const orm::DrogonDbException &) {};

std::string Bot::get_link(const HttpRequestPtr &req) const {
    return "http://"+req->getHeader("Host")+"/bots/"+std::to_string(app_id)+"/detail";
}

std::string SessionData::discord_fullname() {
    return discord_username+'#'+discord_discriminator;
}

void LoginFilter::doFilter(const HttpRequestPtr &req, FilterCallback &&fcb, FilterChainCallback &&fccb) {
    if (getSessionData(req->session())) {
        fccb();
    } else {
        authenticate(fcb);
    }
}

views::views() {
    db = drogon::app().getDbClient();
    moderators = MODERATORS;
}

std::string views::htmlBr(const std::string& src) {
    std::ostringstream fres;
    for (const auto& character : src) {
        if (character == '\n') {
            fres << "<br>";
        } else {
            fres << character;
        }
    }
    return fres.str();
}

void views::start(
        const HttpRequestPtr&, std::function<void (const HttpResponsePtr &)> &&callback
        )
{
    callback(HttpResponse::newRedirectionResponse("/bots/@all", HttpStatusCode::k301MovedPermanently));
}

void views::menu(
        const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)> &&callback
        )
{
    auto data = HttpViewDataPrep(req);
    data.insert("ref", req->getHeader("Referer"));

    callback(HttpResponse::newHttpViewResponse("menu.csp", data));
}

void views::botlist(
        const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)> &&callback,
        std::string order_by)
{
    auto sessionData = getSessionData(req->session());
    auto justMine = req->getPath() == "/bots/@me";
    auto modView = req->getPath() == "/bots/@unapproved";

    if (modView and not sessionData->moderator) {
        callback(HttpResponse::newNotFoundResponse());
        return;
    }

    // Validate order_by string
    if (order_by.empty()) {
        order_by = "0";
    }
    for (const auto character : order_by) {
        if (not std::isalpha(character)) {
            order_by = "votes";
        }
    }

    std::string q = "SELECT * FROM bots WHERE ";
    if (justMine) {
        q.append("owner_id = '"+std::to_string(sessionData->discord_id)+"'");
    } else {
        q.append("approved = "+std::string(modView?"false":"true"));
    }
    q.append(" ORDER BY "+order_by+" DESC");

    db->execSqlAsync(q,
                     [justMine, modView, sessionData, callback] (const orm::Result &rows) {
        std::map<uint64_t, Bot> bot_list;
        for (const auto& r : rows) {
            Bot bot = deserializeBot(r);
            bot_list[bot.app_id] = bot;
        }

        auto data = HttpViewDataPrep(sessionData);
        data.insert("modView", modView);
        data.insert("justMine", justMine);
        data.insert("authed", bool(sessionData));
        data.insert("bots", bot_list);

        callback(HttpResponse::newHttpViewResponse("botlist.csp", data));
    }, dbErr);
}

void views::botdetail(
        const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)> &&callback,
        uint64_t bot_id)
{
    db->execSqlAsync("SELECT * FROM bots WHERE app_id = '"+std::to_string(bot_id)+"'",
                     [this, req, callback, bot_id] (const orm::Result &rows) {
        if (rows.empty()) {
            // Bot not found
            callback(HttpResponse::newNotFoundResponse());
        } else {
            // Bot found
            auto sessionData = getSessionData(req->session());
            auto bot = deserializeBot(rows[0]);
            auto data = HttpViewDataPrep(sessionData);
            data.insert("bot", bot);
            if (not sessionData) {
                // Vote button is not grezed out if user isn"t logged in
                data.insert("canVote", bot.approved);
            } else cantVote(voteID(sessionData->discord_id, bot_id), {
                data.insert("canVote", false);
            } else {
                data.insert("canVote", bot.approved);
            })

            callback(HttpResponse::newHttpViewResponse("botdetail.csp", data));
        }
    }, dbErr);
}

void views::botvote(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)> &&callback,
             uint64_t bot_id) {
    // Check if user is able to vote again
    auto user_id = getSessionData(req->session())->discord_id;
    auto vote_id = voteID(user_id, bot_id);
    cantVote(vote_id, {
        callback(HttpResponse::newRedirectionResponse("detail"));
        return;
    })
    // Register vote
    db->execSqlAsync("UPDATE bots SET votes = votes + 1 WHERE app_id = '"+std::to_string(bot_id)+"'",
                     [this, vote_id, callback] (const orm::Result &rows) {
        if (rows.affectedRows() == 0) {
            // Bot not found
            callback(HttpResponse::newNotFoundResponse());
        } else {
            last_votes[vote_id] = trantor::Date::date();
            // Redirect back
            callback(HttpResponse::newRedirectionResponse("detail"));
        }
    }, [callback] (const orm::DrogonDbException &) {
        callback(HttpResponse::newNotFoundResponse());
    });
}

void views::botedit(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)> &&callback,
             uint64_t bot_id, const std::string& action) {
    auto sessionData = getSessionData(req->session());
    db->execSqlAsync("SELECT * FROM bots WHERE app_id = '"+std::to_string(bot_id)+"'",
                     [this, sessionData, callback, action, req] (const orm::Result &rows) {
        if (rows.empty()) {
            callback(HttpResponse::newNotFoundResponse());
        } else {
            auto bot = deserializeBot(rows[0]);
            // Check if user is botowner or moderator
            if (sessionData->discord_id == bot.owner_id or sessionData->moderator) {
                auto final = [callback] () {
                    callback(HttpResponse::newRedirectionResponse("../detail"));
                };
                // Get action
                if (action == "edit") {
                    switch (req->getMethod()) {
                    case Get: {
                        auto data = HttpViewDataPrep(sessionData);
                        data.insert("bot", bot);

                        callback(HttpResponse::newHttpViewResponse("botedit.csp", data));
                    } break;
                    case Post: {
                        bool refresh = req->getParameter("refresh") == "on";
                        db->execSqlAsync("UPDATE bots SET short_description = '"+dbEsc(req->getParameter("short_description"))+"',"
                                                         "long_description = '"+dbEsc(req->getParameter("long_description"))+"',"
                                                         "support_server = '"+dbEsc(req->getParameter("support_server"))+"',"
                                                         "prefix = '"+dbEsc(req->getParameter("prefix"))+"' "
                                               "WHERE app_id = '"+std::to_string(bot.app_id)+"'",
                                         [this, req, sessionData, refresh, bot, final] (const orm::Result &) {
                            if (refresh) {
                                auto app_id = bot.app_id;
                                auto owner_id = bot.owner_id;
                                getUser(app_id, [this, sessionData, app_id, owner_id, final] (const Json::Value& botuser) {
                                    if (not botuser.empty()) {
                                        db->execSqlAsync("UPDATE bots SET name = '"+dbEsc(botuser["username"].asString())+"',"
                                                                         "avatar_url = '"+dbEsc(botuser["avatar_url"].asString())+
                                                                         std::string(sessionData->discord_id==owner_id?
                                                                                         ("',owner = '"+dbEsc(sessionData->discord_fullname())+"' "):
                                                                                         ("' "))+
                                                               "WHERE app_id = '"+std::to_string(app_id)+"'",
                                                         [final] (const orm::Result &) {
                                            final();
                                       }, dbErr);
                                    } else {
                                        final();
                                    }
                                });
                            } else {
                                final();
                            }
                            // Send webhook log message
                            webhookSend(WebhookEvent{
                                            .title = "Bot updated",
                                            .bot = bot
                                        }, req);
                        }, dbErr);
                    } break;
                    default: break;
                    }
                } else if (action == "delete") {
                    switch (req->getMethod()) {
                    case Get: {
                        auto data = HttpViewDataPrep(sessionData);
                        data.insert("botname", HttpViewData::htmlTranslate(bot.name));
                        callback(HttpResponse::newHttpViewResponse("botdelete.csp", data));

                    } break;
                    case Post: {
                        db->execSqlAsync("DELETE FROM bots WHERE app_id = '"+std::to_string(bot.app_id)+"'",
                                         [bot, req, callback] (const orm::Result &) {
                            // Send webhook log message
                            webhookSend(WebhookEvent{
                                            .title = "Bot deleted",
                                            .bot = bot
                                        }, req);
                            // Return to start page
                            toStartPage(callback);
                        }, dbErr);
                    } break;
                    default: break;
                    }
                } else if (not sessionData->moderator) {
                    goto else_part;
                } else if (action == "approve") {
                    db->execSqlAsync("UPDATE bots SET approved = 't' "
                                           "WHERE app_id = '"+std::to_string(bot.app_id)+"'",
                                     [bot, req, final] (const orm::Result &) {
                        final();
                        // Send webhook log message
                        webhookSend(WebhookEvent{
                                        .title = "Bot approved",
                                        .bot = bot
                                    }, req);
                    }, dbErr);
                } else {
                    else_part:
                    callback(HttpResponse::newNotFoundResponse());
                }
            }
        }
    }, dbErr);
}

void views::botregister_view(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)> &&callback,
                             const std::string& error) {
    // Display page
    auto data = HttpViewDataPrep(req);
    data.insert("error", error);

    callback(HttpResponse::newHttpViewResponse("botregister.csp", data));
}
void views::botregister_submit(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)> &&callback
                               ) {
    auto session = req->session();
    // Set error handler
    auto onError = [callback] (const std::string& e) {
       callback(HttpResponse::newRedirectionResponse("register?error="+e));
    };
    // Get and check parameters
    string short_description, long_description, support_server, prefix;
    uint64_t app_id;
    try {
        app_id = std::stoul(req->getParameter("app_id"));
        short_description = req->getParameter("short_description");
        long_description = req->getParameter("long_description");
        support_server = req->getParameter("support_server");
        prefix = req->getParameter("prefix");
    } catch (std::exception& e) {
        onError(e.what());
    }
    // Check if bot already exists
    db->execSqlAsync("select 1 from bots where app_id ='"+std::to_string(app_id)+"'",
                     [=] (const orm::Result &r) {
        if (not r.empty()) {
            onError("Bot%20has%20already%20been%20registered");
            return;
        }
        // Get bots avatar
        getUser(app_id, [=] (const Json::Value& botuser) {
            // Check result
            if (botuser.empty() or not botuser["bot"].asBool()) {
                onError("Invalid%20client%20ID");
                return;
            }
            // Get session data
            auto sessionData = getSessionData(session);
            // Create bot object
            Bot bot = {
                .name = botuser["username"].asString(),
                .short_description = std::move(short_description),
                .long_description = std::move(long_description),
                .avatar_url = botuser["avatar_url"].asString(),
                .owner = sessionData->discord_fullname(),
                .support_server = std::move(support_server),
                .prefix = std::move(prefix),
                .owner_id = sessionData->discord_id,
                .app_id = app_id,
            };
            // Perform database operation
            db->execSqlAsync(fmt::format("INSERT INTO bots (name, short_description, long_description, avatar_url, owner, support_server, prefix, owner_id, app_id, votes, approved) "
                                         "VALUES('{}', '{}', '{}', '{}', '{}', '{}', '{}', '{}', '{}', 0, 'f')",
                             dbEsc(bot.name), dbEsc(bot.short_description), dbEsc(bot.long_description), dbEsc(bot.avatar_url), bot.owner, dbEsc(bot.support_server), dbEsc(bot.prefix), bot.owner_id, bot.app_id),
                             [req, bot, sessionData, callback] (const orm::Result &) {
                callback(HttpResponse::newRedirectionResponse(std::to_string(bot.app_id)+"/detail"));
                // Send webhook log message
                webhookSend(WebhookEvent{
                                .title = "Bot is waiting for approval",
                                .bot = bot
                            }, req);
            }, dbErr);
        });
    }, dbErr);
}

void views::discorddeauth(
        const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)> &&callback
        ) {
    req->session()->clear();
    toStartPage(callback);
}

void views::discordauth(
        const HttpRequestPtr& client_req, std::function<void (const HttpResponsePtr &)> &&callback,
        const std::string& code)
{
    if (code.empty()) {
        authenticate(callback);
    } else {
        // Get token from API
        auto discordapi = HttpClient::newHttpClient("https://discord.com");
        auto req = HttpRequest::newHttpRequest();
        req->setPath("/api/v8/oauth2/token");
        req->setContentTypeCode(ContentType::CT_APPLICATION_X_FORM);
        req->setMethod(HttpMethod::Post);
        {
            req->setParameter("client_id", CLIENT_ID);
            req->setParameter("client_secret", CLIENT_SECRET);
            req->setParameter("redirect_uri", REDIRECT_URI);
            req->setParameter("grant_type", "authorization_code");
            req->setParameter("scope", "identify");
            req->setParameter("code", code);
        }
        discordapi->sendRequest(req, [this, discordapi, client_req, callback] (ReqResult, const HttpResponsePtr &response) {
            // Check for success
            if (response->getStatusCode() == HttpStatusCode::k200OK) {
                // Auth success
                auto &data = *response->getJsonObject().get();
                // Prepare session
                auto session = client_req->session();
                resetSessionData(session);
                auto sessionData = std::make_shared<SessionData>();
                sessionData->discord_access_token = data["access_token"].asString();
                // Get user data
                auto req = HttpRequest::newHttpRequest();
                req->setPath("/api/v8/users/@me");
                req->setMethod(HttpMethod::Get);
                req->addHeader("Authorization", "Bearer "+sessionData->discord_access_token);
                discordapi->sendRequest(req, [this, client_req, callback, session, sessionData] (ReqResult, const HttpResponsePtr &response) {
                    if (response->getStatusCode() == HttpStatusCode::k200OK) {
                        // Getting user data success
                        auto &userdata = *response->getJsonObject().get();
                        sessionData->discord_id = std::stoul(userdata["id"].asString());
                        sessionData->discord_username = userdata["username"].asString();
                        sessionData->discord_discriminator = userdata["discriminator"].asString();
                        sessionData->moderator = std::find(moderators.begin(), moderators.end(), sessionData->discord_id) != moderators.end();
                        setSessionData(session, sessionData);
                        // Show success page
                        auto data = HttpViewDataPrep(sessionData);
                        callback(HttpResponse::newHttpViewResponse("authsuccess.csp", data));
                    } else {
                        toStartPage(callback);
                    }
                });
            } else {
                toStartPage(callback);
            }
        });
    }
}
