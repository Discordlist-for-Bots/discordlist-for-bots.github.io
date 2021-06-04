#pragma once
#include <memory>
#include <drogon/HttpController.h>
using namespace std;


struct Bot {
    string name, short_description, long_description, avatar_url, owner, support_server, prefix;
    uint64_t owner_id, app_id;
    int64_t votes = 0;
    bool approved = false;

    std::string get_link(const drogon::HttpRequestPtr &req) const;
};

struct SessionData {
    uint64_t discord_id;
    std::string discord_access_token,
                discord_username, discord_discriminator;
    bool moderator;
    std::string discord_fullname();
};
using SessionDataPtr = std::shared_ptr<SessionData>;

class LoginFilter:public drogon::HttpFilter<LoginFilter>
{
public:
    virtual void doFilter(const drogon::HttpRequestPtr &,
                          drogon::FilterCallback &&,
                          drogon::FilterChainCallback &&) override;
};

using namespace drogon;
class views: public drogon::HttpController<views> {
    orm::DbClientPtr db;
    std::unordered_map<std::string, trantor::Date> last_votes;
    std::vector<uint64_t> moderators;
public:
    views();
    static std::string htmlBr(const std::string&);
    void start(const HttpRequestPtr&, std::function<void (const HttpResponsePtr &)> &&);
    void menu(const HttpRequestPtr&, std::function<void (const HttpResponsePtr &)> &&);
    void botlist(const HttpRequestPtr&, std::function<void (const HttpResponsePtr &)> &&, std::string order_by);
    void botdetail(const HttpRequestPtr&, std::function<void (const HttpResponsePtr &)> &&, uint64_t);
    void botvote(const HttpRequestPtr&, std::function<void (const HttpResponsePtr &)> &&, uint64_t);
    void botedit(const HttpRequestPtr&, std::function<void (const HttpResponsePtr &)> &&, uint64_t, const std::string&);
    void botregister_view(const HttpRequestPtr&, std::function<void (const HttpResponsePtr &)> &&, const std::string&);
    void botregister_submit(const HttpRequestPtr&, std::function<void (const HttpResponsePtr &)> &&);
    void discordauth(const HttpRequestPtr&, std::function<void (const HttpResponsePtr &)> &&, const std::string&);
    void discorddeauth(const HttpRequestPtr&, std::function<void (const HttpResponsePtr &)> &&);

    METHOD_LIST_BEGIN
    ADD_METHOD_TO(views::start, "/", Get);
    ADD_METHOD_TO(views::botlist, "/bots/@all?o={}", Get);
    ADD_METHOD_TO(views::botlist, "/bots/@me?o={}", Get, "LoginFilter");
    ADD_METHOD_TO(views::botlist, "/bots/@unapproved?o={}", Get, "LoginFilter");
    ADD_METHOD_TO(views::botregister_view, "/bots/register?error={1}", Get, "LoginFilter");
    ADD_METHOD_TO(views::botregister_submit, "/bots/register", Post, "LoginFilter");
    ADD_METHOD_TO(views::botdetail, "/bots/{1}/detail", Get);
    ADD_METHOD_TO(views::botvote, "/bots/{1}/vote", Get, "LoginFilter");
    ADD_METHOD_TO(views::botedit, "/bots/{1}/edit/{2}", Get, Post, "LoginFilter");
    ADD_METHOD_TO(views::discordauth, "/discordauth?code={1}", Get);
    ADD_METHOD_TO(views::discorddeauth, "/discorddeauth", Get);
    ADD_METHOD_TO(views::menu, "/menu", Get);
    METHOD_LIST_END
};
