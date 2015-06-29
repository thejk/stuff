#include "common.hh"

#include <algorithm>
#include <cinttypes>
#include <iostream>
#include <memory>
#include <string>

#include "auth.hh"
#include "cgi.hh"
#include "config.hh"
#include "db.hh"
#include "event.hh"
#include "event_utils.hh"
#include "http.hh"
#include "sender_client.hh"

using namespace stuff;

namespace {

std::unique_ptr<Config> g_cfg;
std::unique_ptr<SenderClient> g_sender;

class Page;

Page* g_page;

class Page {
public:
    Page(const std::string& channel)
        : channel_(channel) {
        assert(!g_page);
        g_page = this;
        header();
    }

    ~Page() {
        footer();
        assert(g_page == this);
        g_page = nullptr;
        std::map<std::string, std::string> headers;
        headers.insert(
                std::make_pair("Content-Type", "text/html; charset=utf-8"));
        Http::response(200, headers, content_);
    }

    void write(const std::string& text) {
        content_.append(text);
    }

    void write_safe(const std::string& text) {
        auto last = text.begin();
        std::string tmp;
        for (auto it = last; it != text.end(); ++it) {
            if (!is_safe(*it, &tmp)) {
                content_.append(last, it);
                content_.push_back('&');
                content_.append(tmp);
                content_.push_back(';');
                last = it + 1;
            }
        }
        content_.append(last, text.end());
    }

private:
    static bool is_safe(char c, std::string* out) {
        switch (c) {
        case '<':
            out->assign("lt");
            return false;
        case '>':
            out->assign("gt");
            return false;
        case '&':
            out->assign("amp");
            return false;
        case '"':
            out->assign("quot");
            return false;
        default:
            return true;
        }
    }

    void header() {
        write("<!doctype html>");
        write("<html>");
        write("<head>");
        write("<title>Event for ");
        write_safe(channel_);
        write("</title>");
        std::string stylesheet;
        if (g_cfg) stylesheet = g_cfg->get("stylesheet", "");
        if (!stylesheet.empty()) {
            write("<link rel=\"stylesheet\" href=\"");
            write_safe(stylesheet);
            write("\">");
        }
        write("</head>");
        write("<body>");
        write("<div id=\"content\">");
    }

    void footer() {
        write("</div>");
        write("</body>");
        write("</html>");
    }

    std::string channel_;
    std::string content_;
};

void error_response(const std::string& message) {
    g_page->write("<h2>Error</h2>");
    g_page->write("<p>");
    g_page->write_safe(message);
    g_page->write("</p>");
}

std::string get_channel(CGI* cgi) {
    auto path = cgi->request_path();
    if (path.empty()) return path;
    size_t end = path.size();
    if (path.back() == '/') end--;
    size_t start = 0;
    if (path.front() == '/') start++;
    return path.substr(start, end - start);
}

bool show(CGI* cgi, EventUtils* utils, const std::string& user) {
    Page page(utils->channel());
    auto event = utils->next();
    if (!utils->good()) return true;
    if (!event) {
        page.write("<h2>No event scheduled</h2>");
        return true;
    }
    page.write("<h1>");
    page.write_safe(event->name());
    page.write(" @ ");
    page.write_safe(EventUtils::format_date(event->start()));
    page.write("</h1>");
    page.write("<p>");
    page.write_safe(event->text());
    page.write("</p>");
    std::vector<Event::Going> going;
    event->going(&going);
    int8_t state = 0;
    std::string note;
    if (going.empty()) {
        page.write("<p>Expect no-one</p>");
    } else {
        page.write("<p id=\"going\">");
        auto it = going.begin();
        for (; it != going.end(); ++it) {
            if (!it->is_going) break;
            if (it->name == user) {
                state = 1;
                note = it->note;
            }
            page.write("<span class=\"name\">");
            page.write_safe(it->name);
            page.write("</span>");
            if (!it->note.empty()) {
                page.write("&nbsp;<span class=\"note\">");
                page.write_safe(it->note);
                page.write("</span>");
            }
            page.write("<br>");
        }
        if (it != going.end()) {
            page.write("</p><h3>Not going</h3><p id=\"not_going\">");
            for (; it != going.end(); ++it) {
                if (it->name == user) {
                    state = -1;
                    note = it->note;
                }
                page.write("<span class=\"name\">");
                page.write_safe(it->name);
                page.write("</span>");
                if (!it->note.empty()) {
                    page.write("&nbsp;<span class=\"note\">");
                    page.write_safe(it->note);
                    page.write("</span>");
                }
                page.write("<br>");
            }
        }
    }
    page.write("</p>");
    page.write("<form action=\"");
    page.write_safe(cgi->request_uri());
    page.write("\" method=\"post\">");
    page.write("<input type=\"hidden\" name=\"id\" value=\"");
    char tmp[100];
    snprintf(tmp, sizeof(tmp), "%" PRId64, event->id());
    page.write_safe(tmp);
    page.write("\">");
    page.write("<p>");
    if (state == 0) {
        page.write("<input type=\"submit\" name=\"going\" value=\"Going\">");
        page.write("<input type=\"submit\" name=\"not_going\" value=\"Not going\">");
        page.write("<br>");
        page.write("<input type=\"text\" name=\"note\" value=\"\">");
    } else if (state < 0) {
        page.write("<input type=\"submit\" name=\"going\" value=\"Going\">");
        page.write("<br>");
        page.write("<input type=\"text\" name=\"note\" value=\"");
        page.write_safe(note);
        page.write("\">");
    } else /* if (state > 0) */ {
        page.write("<input type=\"submit\" name=\"not_going\" value=\"Not going\">");
        page.write("<br>");
        page.write("<input type=\"text\" name=\"note\" value=\"");
        page.write_safe(note);
        page.write("\">");
    }
    page.write("</p>");
    page.write("</form>");
    return true;
}

bool going(CGI* cgi, EventUtils* utils, const std::string& user,
           const std::map<std::string, std::string>& data) {
    bool const is_going = !data.at("going").empty();
    std::string note;
    auto it = data.find("note");
    if (it != data.end()) note = it->second;
    char* end = nullptr;
    long long tmp;
    it = data.find("id");
    if (it != data.end()) {
        errno = 0;
        tmp = strtoll(it->second.c_str(), &end, 10);
    }
    if (errno == 0 && end && !*end) {
        auto events = utils->all();
        if (!utils->good()) return true;
        for (auto& event : events) {
            if (event->id() == tmp) {
                event->update_going(user, is_going, note);
                if (event->store()) {
                    utils->going(event.get(), is_going, user,
                                 cgi->remote_addr());
                }
            }
        }
    }
    // Redirect the POST response to a GET request
    auto link = cgi->request_uri();
    std::map<std::string, std::string> headers;
    headers.insert(std::make_pair("Content-Type", "text/plain"));
    headers.insert(std::make_pair("Location", link));
    Http::response(303, headers, link);
    return true;
}

bool handle_request(CGI* cgi) {
    switch (cgi->request_type()) {
    case CGI::GET:
    case CGI::POST:
        break;
    default:
        Http::response(500, "Unsupported request");
        return true;
    }
    auto channel = get_channel(cgi);
    if (channel.empty()) {
        Http::response(500, "Bad channel");
        return true;
    }
    std::string passwd;
    bool ok_passwd = false;
    if (g_cfg) {
        passwd = g_cfg->get(channel, "bad");
        if (passwd != "bad") {
            ok_passwd = true;
        } else {
            passwd = g_cfg->get(channel, "bad2");
            if (passwd != "bad2") {
                ok_passwd = true;
            }
        }
    }
    if (!ok_passwd) {
        Http::response(500, "Bad channel");
        return true;
    }
    std::string user;
    if (!Auth::auth(cgi, "Event for " + channel, passwd, &user)) {
        return true;
    }
    auto utils = EventUtils::create(channel, error_response, g_cfg.get(),
                                    g_sender.get());
    std::map<std::string, std::string> data;
    cgi->get_data(&data);
    if (!data["going"].empty() || !data["not_going"].empty()) {
        return going(cgi, utils.get(), user, data);
    }
    return show(cgi, utils.get(), user);
}

}  // namespace

int main() {
    g_cfg = Config::create();
    if (!g_cfg->load("./page.config")) {
        g_cfg->load(SYSCONFDIR "/page.config");
    }
    g_sender = SenderClient::create(g_cfg.get());
    int ret = CGI::run(handle_request);
    g_sender.reset();
    g_cfg.reset();
    return ret;
}
