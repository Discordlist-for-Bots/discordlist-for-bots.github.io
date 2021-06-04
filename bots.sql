--
-- PostgreSQL database dump
--

-- Dumped from database version 13.1 (Ubuntu 13.1-1.pgdg20.04+1)
-- Dumped by pg_dump version 13.1 (Ubuntu 13.1-1.pgdg20.04+1)

SET statement_timeout = 0;
SET lock_timeout = 0;
SET idle_in_transaction_session_timeout = 0;
SET client_encoding = 'UTF8';
SET standard_conforming_strings = on;
SELECT pg_catalog.set_config('search_path', '', false);
SET check_function_bodies = false;
SET xmloption = content;
SET client_min_messages = warning;
SET row_security = off;

SET default_tablespace = '';

SET default_table_access_method = heap;

--
-- Name: bots; Type: TABLE; Schema: public; Owner: oaayooms
--

CREATE TABLE public.bots (
    name text,
    short_description text,
    long_description text,
    avatar_url text,
    owner text,
    support_server text,
    prefix text,
    owner_id text,
    app_id text,
    votes bigint,
    approved boolean,
    verified boolean,
    CONSTRAINT can_vote_check CHECK ((((votes > 0) AND approved) OR (votes < 1)))
);


ALTER TABLE public.bots OWNER TO oaayooms;

--
-- Data for Name: bots; Type: TABLE DATA; Schema: public; Owner: oaayooms
--

COPY public.bots (name, short_description, long_description, avatar_url, owner, support_server, prefix, owner_id, app_id, votes, approved) FROM stdin;
DFB	This Bot is for the Server DFB	This bot was created for our Discordlist for Bots Server	https://cdn.discordapp.com/avatars/795612465130897420/c3bd0733f876a664b4b79ec03866f131.png	Julius#1755	42vDtZxZSt	dfb?	703944517048598568	795612465130897420	3744	t
Tuxiflux	A fun but simple bot with globalchat	Tuxiflux is a funny, useful and intuitive bot for server moderation and play with in-bot money.	https://cdn.discordapp.com/embed/avatars/2.png	Tuxifan#4660	6smrmKkjP7	t#	609486822715818000	788310535799308288	9	t
MEE6	szreszxer	eszxertgszx	https://cdn.discordapp.com/avatars/159985870458322944/b50adff099924dd5e6b72d13f77eb9d7.png	Tuxifan#4660	erdsxzt	redsxz	609486822715818000	159985870458322944	0	f
\.


--
-- PostgreSQL database dump complete
--

