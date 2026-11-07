/* http.h - HTTP/1.0 клиент поверх TCP из net.h: URL, редиректы, chunked. Чистый C. */
#ifndef HTTP_H
#define HTTP_H
#define HE_URL     -1
#define HE_HTTPS   -2
#define HE_DNS     -3
#define HE_CONNECT -4
#define HE_SEND    -5
#define HE_NORESP  -6
#define HE_ABORT   -7
#define HE_LOOP    -8
#define HE_TLS     -9

static int (*http_abort)(void);
static const char *http_accept = "text/html, text/plain";
static int http_doh_on = 1;            /* запасной DNS через HTTPS, если обычный не дал адрес */
static int dns_via_doh, doh_last_err;
static u32 http_resolve(const char *name);
static u8 *http_buf;
static int http_max;
static u8 *http_raw;                  /* сырой TCP-поток для https (если NULL - https недоступен) */
static int http_rawmax;
static void (*http_rand)(u8 *, int);

struct hres {
    int status, html, body_off, body_len, truncated;   /* html: 1 - HTML, 0 - текст, -1 - бинарные данные */
    char loc[256], url[256], ctype[48];
};

static int h_len(const char *s) { int n = 0; while (s[n]) n++; return n; }
static char h_lc(char c) { return (c >= 'A' && c <= 'Z') ? c + 32 : c; }
static int ci_prefix(const char *s, const char *p) {
    while (*p) { if (h_lc(*s) != h_lc(*p)) return 0; s++; p++; }
    return 1;
}
static void h_apnd(char *d, int *n, int max, const char *s) {
    while (*s && *n < max - 1) d[(*n)++] = *s++;
    d[*n] = 0;
}
static const char *http_err_str(int e) {
    switch (e) {
        case HE_URL: return "Invalid or unsupported URL";
        case HE_HTTPS: return "HTTPS is not available in this build.";
        case HE_TLS:
#ifdef HTTP_TLS
            return tls_err_str();
#else
            return "TLS error";
#endif
        case HE_DNS:
            switch (dns_why) {
                case 1: return "Cannot resolve the name: no DNS server is configured. Run 'dhcp' or 'dns 8.8.8.8'.";
                case 2: return "Cannot resolve the name: the DNS server is unreachable. Wrong network settings? Run 'dhcp' (bridged/real network) or check 'ifconfig'.";
                case 3: return "Cannot resolve the name: the DNS server did not answer. Try 'dhcp', or set another one, e.g. 'dns 8.8.8.8'.";
                case 4: return "The DNS servers say this host name does not exist (NXDOMAIN). If it surely exists, run 'netdiag'.";
                case 5: return "This name has no IPv4 address.";
                case 6: return "The DNS server returned an error (not NXDOMAIN). Run 'netdiag' to see which one.";
            }
            return "Cannot resolve the host name (DNS failed)";
        case HE_CONNECT: return "Cannot connect to the server (no route, refused or timed out)";
        case HE_SEND: return "Failed to send the request";
        case HE_NORESP: return "The server did not send a response";
        case HE_ABORT: return "Cancelled";
        case HE_LOOP: return "Too many redirects";
    }
    return "Error";
}

static int http_parse_url(const char *u, int *https, char *host, int hmax, int *port, char *path, int pmax) {
    *https = 0;
    if (ci_prefix(u, "http://")) u += 7;
    else if (ci_prefix(u, "https://")) { *https = 1; u += 8; }
    int h = 0;
    while (*u && *u != '/' && *u != ':' && *u != '?' && *u != '#') {
        if (h + 1 >= hmax) return -1;
        host[h++] = *u++;
    }
    host[h] = 0;
    if (!h) return -1;
    *port = *https ? 443 : 80;
    if (*u == ':') {
        u++;
        int p = 0;
        while (*u >= '0' && *u <= '9') { p = p * 10 + (*u - '0'); u++; if (p > 65535) return -1; }
        if (!p) return -1;
        *port = p;
    }
    int k = 0;
    if (*u != '/') path[k++] = '/';
    while (*u && *u != '#') {
        if (k + 1 >= pmax) return -1;
        path[k++] = (*u == ' ') ? '+' : *u;
        u++;
    }
    path[k] = 0;
    return 0;
}

/* ---- относительные ссылки ---- */
static int url_has_scheme(const char *h) {
    int i = 0;
    while ((h[i] >= 'a' && h[i] <= 'z') || (h[i] >= 'A' && h[i] <= 'Z') || (h[i] >= '0' && h[i] <= '9') || h[i] == '+' || h[i] == '-' || h[i] == '.') i++;
    return i > 0 && h[i] == ':';
}
static void url_norm(char *u) {                    /* убирает #фрагмент, сворачивает . и .. в пути */
    for (char *f = u; *f; f++) if (*f == '#') { *f = 0; break; }
    char *p = u;
    while (*p && !(p[0] == ':' && p[1] == '/' && p[2] == '/')) p++;
    if (!*p) return;
    p += 3;
    while (*p && *p != '/' && *p != '?') p++;
    if (*p != '/') return;
    char *ps = p, *qs = p, query[260];
    while (*qs && *qs != '?') qs++;
    int ql = 0;
    while (qs[ql] && ql < 258) { query[ql] = qs[ql]; ql++; }
    query[ql] = 0;
    *qs = 0;
    char out[300]; int on = 0, ns = 0, st[40], i = 0;
    while (ps[i]) {
        i++;
        int s = i;
        while (ps[i] && ps[i] != '/') i++;
        int L = i - s, last = !ps[i];
        if (L == 0) { if (last && on < 298) out[on++] = '/'; continue; }
        if (L == 1 && ps[s] == '.') { if (last && on < 298) out[on++] = '/'; continue; }
        if (L == 2 && ps[s] == '.' && ps[s + 1] == '.') {
            if (ns > 0) on = st[--ns];
            if (last && on < 298) out[on++] = '/';
            continue;
        }
        if (on + L + 2 >= 298) break;
        if (ns < 40) st[ns++] = on;
        out[on++] = '/';
        for (int k = 0; k < L; k++) out[on++] = ps[s + k];
    }
    if (on == 0) out[on++] = '/';
    out[on] = 0;
    int k = 0;
    for (int j = 0; out[j]; j++) ps[k++] = out[j];
    for (int j = 0; query[j]; j++) ps[k++] = query[j];
    ps[k] = 0;
}
static int url_resolve(const char *base, const char *href, char *out, int max) {
    char tmp[600]; int n = 0;
    while (*href == ' ') href++;
    if (!*href) return -1;
    tmp[0] = 0;
    if (ci_prefix(href, "http://") || ci_prefix(href, "https://") || ci_prefix(href, "file://")) {
        h_apnd(tmp, &n, sizeof tmp, href);
    } else if (url_has_scheme(href)) {
        return -1;                                   /* mailto:, javascript: и т.п. */
    } else {
        int sl;
        if (ci_prefix(base, "https://")) sl = 8; else if (ci_prefix(base, "http://")) sl = 7;
        else if (ci_prefix(base, "file://")) sl = 7; else return -1;
        int ol = sl;
        while (base[ol] && base[ol] != '/' && base[ol] != '?' && base[ol] != '#') ol++;
        if (href[0] == '/' && href[1] == '/') {
            for (int i = 0; i < sl - 2; i++) tmp[n++] = base[i];      /* "http:" */
            tmp[n] = 0; h_apnd(tmp, &n, sizeof tmp, href);
        } else if (href[0] == '/') {
            for (int i = 0; i < ol; i++) tmp[n++] = base[i];
            tmp[n] = 0; h_apnd(tmp, &n, sizeof tmp, href);
        } else if (href[0] == '#') {
            h_apnd(tmp, &n, sizeof tmp, base);
        } else if (href[0] == '?') {
            int e = ol;
            while (base[e] && base[e] != '?' && base[e] != '#') e++;
            for (int i = 0; i < e; i++) tmp[n++] = base[i];
            tmp[n] = 0; h_apnd(tmp, &n, sizeof tmp, href);
        } else {
            int e = ol, ls = -1;
            while (base[e] && base[e] != '?' && base[e] != '#') { if (base[e] == '/') ls = e; e++; }
            if (ls < 0) { for (int i = 0; i < ol; i++) tmp[n++] = base[i]; tmp[n++] = '/'; }
            else for (int i = 0; i <= ls; i++) tmp[n++] = base[i];
            tmp[n] = 0; h_apnd(tmp, &n, sizeof tmp, href);
        }
    }
    if (n >= max) return -1;
    url_norm(tmp);
    int k = 0;
    while (tmp[k]) { out[k] = tmp[k]; k++; }
    out[k] = 0;
    return 0;
}

/* ---- разбор ответа ---- */
static int http_find_hdr_end(const u8 *b, int len, int from) {
    for (int i = from; i + 3 < len; i++)
        if (b[i] == '\r' && b[i + 1] == '\n' && b[i + 2] == '\r' && b[i + 3] == '\n') return i + 4;
    return -1;
}
static int http_hdr(const u8 *b, int hdr_end, const char *name, char *out, int max) {
    int i = 0;
    while (i < hdr_end && b[i] != '\n') i++;           /* пропускаем строку статуса */
    i++;
    while (i < hdr_end) {
        int e = i;
        while (e < hdr_end && b[e] != '\n') e++;
        if (ci_prefix((const char *)b + i, name)) {
            int p = i + h_len(name);
            while (p < e && (b[p] == ' ' || b[p] == '\t')) p++;
            int k = 0;
            while (p < e && b[p] != '\r' && k < max - 1) out[k++] = b[p++];
            out[k] = 0;
            return 1;
        }
        i = e + 1;
    }
    return 0;
}
static int http_dechunk(u8 *b, int off, int len) {
    int src = off, dst = off, end = off + len;
    while (src < end) {
        u32 sz = 0; int digits = 0;
        while (src < end) {
            char c = h_lc(b[src]); int v;
            if (c >= '0' && c <= '9') v = c - '0'; else if (c >= 'a' && c <= 'f') v = c - 'a' + 10; else break;
            sz = sz * 16 + v; digits++; src++;
        }
        while (src < end && b[src] != '\n') src++;
        src++;
        if (!digits || sz == 0) break;
        int n = (int)sz;
        if (n > end - src) n = end - src;
        for (int i = 0; i < n; i++) b[dst++] = b[src + i];
        src += n;
        if (src < end && b[src] == '\r') src++;
        if (src < end && b[src] == '\n') src++;
    }
    return dst - off;
}

static int http_tls_len(void) {
#ifdef HTTP_TLS
    return tls_plen;
#else
    return 0;
#endif
}
static int http_tls_done(void) {
#ifdef HTTP_TLS
    return tls_closed;
#else
    return 0;
#endif
}
static int http_tls_over(void) {
#ifdef HTTP_TLS
    return tls_poverflow;
#else
    return 0;
#endif
}
static void http_shutdown(int https) {
#ifdef HTTP_TLS
    if (https) tls_close_notify();
#else
    (void)https;
#endif
    tcp_close();
}
#ifdef HTTP_TLS
static void http_tls_pump(void) { net_poll(); tls_rlen = tcp_blen; }
static int http_tls_send(const u8 *d, int n) { return tcp_send(d, n); }
static int http_tls_dead(void) { return tcp_rst || tcp_fin || tcp_state == TCP_CLOSED; }
#endif

static int http_fetch_req(const char *url, const char *method, const char *ctype, const char *req_body, int req_body_len, struct hres *r) {
    char host[128], path[400];
    int https, port;
    r->status = 0; r->html = 0; r->body_off = r->body_len = r->truncated = 0; r->loc[0] = r->ctype[0] = 0;
    if (http_parse_url(url, &https, host, sizeof host, &port, path, sizeof path) < 0) return HE_URL;
    if (https && !http_raw) return HE_HTTPS;
#ifndef HTTP_TLS
    if (https) return HE_HTTPS;
#endif
    u32 ip;
    if ((host[0] == '1' && host[1] == '2' && host[2] == '7' && host[3] == '.') ||
        (host[0] == 'l' && host[1] == 'o' && host[2] == 'c' && host[3] == 'a' && host[4] == 'l')) {
        ip = (ngw && ((nip >> 8) == 0x0A0002u)) ? ngw : 0x7F000001u;
    } else if (!net_parse_ip(host, &ip)) {
        ip = http_resolve(host);
        if (!ip) return HE_DNS;
    }
    tcp_buf = https ? http_raw : http_buf; tcp_bmax = https ? http_rawmax : http_max;
    if (tcp_connect(ip, (u16)port) < 0) return HE_CONNECT;
#ifdef HTTP_TLS
    if (https) {
        tls_raw = tcp_buf; tls_rlen = 0; tls_rpos = 0;
        tls_plain = http_buf; tls_pmax = http_max;
        tls_pump = http_tls_pump; tls_send_raw = http_tls_send; tls_dead = http_tls_dead; tls_ms = ms_now; tls_rand = http_rand;
        if (tls_handshake(host) < 0) { tcp_close(); return HE_TLS; }
    }
#endif
    char req[800]; int n = 0;
    h_apnd(req, &n, sizeof req, method ? method : "GET");
    h_apnd(req, &n, sizeof req, " ");
    h_apnd(req, &n, sizeof req, path);
    h_apnd(req, &n, sizeof req, " HTTP/1.0\r\nHost: ");
    h_apnd(req, &n, sizeof req, host);
    if ((!https && port != 80) || (https && port != 443)) {
        char ps[12]; int pi = 0;
        ps[pi++] = ':';
        int pv = port, pdiv = 10000;
        while (pdiv > 1 && pv / pdiv == 0) pdiv /= 10;
        while (pdiv >= 1) { ps[pi++] = '0' + (pv / pdiv); pv %= pdiv; pdiv /= 10; }
        ps[pi] = 0;
        h_apnd(req, &n, sizeof req, ps);
    }
    h_apnd(req, &n, sizeof req, "\r\nUser-Agent: Mozilla/5.0 (compatible; Cravgos/0.7.1; x86_64)\r\nAccept: ");
    h_apnd(req, &n, sizeof req, http_accept);
    if (ctype && *ctype) {
        h_apnd(req, &n, sizeof req, "\r\nContent-Type: ");
        h_apnd(req, &n, sizeof req, ctype);
    }
    if (req_body && req_body_len > 0) {
        h_apnd(req, &n, sizeof req, "\r\nContent-Length: ");
        char bls[16]; int bli = 0;
        int bv = req_body_len, bdiv = 10000;
        while (bdiv > 1 && bv / bdiv == 0) bdiv /= 10;
        while (bdiv >= 1) { bls[bli++] = '0' + (bv / bdiv); bv %= bdiv; bdiv /= 10; }
        bls[bli] = 0;
        h_apnd(req, &n, sizeof req, bls);
    }
    h_apnd(req, &n, sizeof req, "\r\nConnection: close\r\n\r\n");
#ifdef HTTP_TLS
    if (https) {
        if (tls_send((const u8 *)req, n) < 0) { tcp_close(); return HE_SEND; }
        if (req_body && req_body_len > 0) { if (tls_send((const u8 *)req_body, req_body_len) < 0) { tcp_close(); return HE_SEND; } }
    } else
#endif
    {
        if (tcp_send((const u8 *)req, n) < 0) { tcp_close(); return HE_SEND; }
        if (req_body && req_body_len > 0) { if (tcp_send((const u8 *)req_body, req_body_len) < 0) { tcp_close(); return HE_SEND; } }
    }
    u32 t_start = ms_now(), t_last = t_start;
    int last_len = 0, hdr_end = -1, clen = -1, scan = 0, rl = 0;
    for (;;) {
        net_poll();
#ifdef HTTP_TLS
        if (https) { tls_rlen = tcp_blen; tls_process(); }
#endif
        rl = https ? http_tls_len() : tcp_blen;
        if (rl != last_len) { last_len = rl; t_last = ms_now(); }
        if (hdr_end < 0) {
            hdr_end = http_find_hdr_end(http_buf, rl, scan);
            scan = rl > 3 ? rl - 3 : 0;
            if (hdr_end >= 0) {
                char v[24];
                if (http_hdr(http_buf, hdr_end, "content-length:", v, sizeof v)) {
                    clen = 0;
                    for (int i = 0; v[i] >= '0' && v[i] <= '9'; i++) clen = clen * 10 + (v[i] - '0');
                }
            }
        }
        if (tcp_fin || tcp_rst || tcp_state == TCP_CLOSED || tcp_overflow || http_tls_done()) break;
        if (hdr_end >= 0 && clen >= 0 && rl - hdr_end >= clen) break;
        if (hdr_end >= 0 && clen < 0 && rl > hdr_end + 4) {
            const u8 *b = http_buf + hdr_end;
            int bl = rl - hdr_end;
            if (bl >= 5 && b[bl - 5] == '\r' && b[bl - 4] == '\n' && b[bl - 3] == '0' && b[bl - 2] == '\r' && b[bl - 1] == '\n') break;
            if (bl >= 7 && b[bl - 7] == '\r' && b[bl - 6] == '\n' && b[bl - 5] == '0' && b[bl - 4] == '\r' && b[bl - 3] == '\n' && b[bl - 2] == '\r' && b[bl - 1] == '\n') break;
        }
        u32 now = ms_now();
        if (now - t_last > 8000 || now - t_start > 40000) break;
        if (http_abort && http_abort()) { http_shutdown(https); return HE_ABORT; }
    }
    http_shutdown(https);
    int total = https ? http_tls_len() : tcp_blen;
    r->truncated = tcp_overflow || http_tls_over();
    if (hdr_end < 0) hdr_end = http_find_hdr_end(http_buf, total, 0);
    if (hdr_end < 0 || total < 12) return HE_NORESP;
    int st = 0, i = 0;
    while (i < total && http_buf[i] != ' ') i++;
    i++;
    while (i < total && http_buf[i] >= '0' && http_buf[i] <= '9') st = st * 10 + (http_buf[i++] - '0');
    r->status = st;
    http_hdr(http_buf, hdr_end, "location:", r->loc, sizeof r->loc);
    http_hdr(http_buf, hdr_end, "content-type:", r->ctype, sizeof r->ctype);
    char te[24];
    int body_len = total - hdr_end;
    if (http_hdr(http_buf, hdr_end, "transfer-encoding:", te, sizeof te) && ci_prefix(te, "chunked"))
        body_len = http_dechunk(http_buf, hdr_end, body_len);
    else if (clen >= 0 && clen < body_len) body_len = clen;
    r->body_off = hdr_end; r->body_len = body_len;
    http_buf[hdr_end + body_len] = 0;
    int is_html = 0, is_text = 0;
    for (int k = 0; r->ctype[k]; k++) {
        if (ci_prefix(r->ctype + k, "html")) is_html = 1;
        if (ci_prefix(r->ctype + k, "text/")) is_text = 1;
    }
    if (!r->ctype[0]) { is_text = 1; is_html = (body_len > 0 && http_buf[hdr_end] == '<'); }
    r->html = is_html ? 1 : (is_text ? 0 : -1);
    return 0;
}
static int http_fetch(const char *url, struct hres *r) {
    return http_fetch_req(url, "GET", 0, 0, 0, r);
}
static int http_post(const char *url, const char *ctype, const char *body, int body_len, struct hres *r) {
    return http_fetch_req(url, "POST", ctype, body, body_len, r);
}
static int http_post_follow(const char *url, const char *ctype, const char *body, int body_len, struct hres *r) {
    char cur[256];
    int k = 0;
    while (url[k] && k < 255) { cur[k] = url[k]; k++; }
    cur[k] = 0;
    int is_post = 1;
    for (int hop = 0; hop < 6; hop++) {
        int e = is_post ? http_fetch_req(cur, "POST", ctype, body, body_len, r) : http_fetch(cur, r);
        for (k = 0; cur[k]; k++) r->url[k] = cur[k];
        r->url[k] = 0;
        if (e < 0) return e;
        if (r->status >= 300 && r->status < 400 && r->loc[0]) {
            char nx[256];
            if (url_resolve(cur, r->loc, nx, sizeof nx) < 0) return HE_URL;
            for (k = 0; nx[k]; k++) cur[k] = nx[k];
            cur[k] = 0;
            if (r->status == 301 || r->status == 302 || r->status == 303) is_post = 0;
            continue;
        }
        return 0;
    }
    return HE_LOOP;
}

static int http_get_follow(const char *url, struct hres *r) {
    char cur[256];
    int k = 0;
    while (url[k] && k < 255) { cur[k] = url[k]; k++; }
    cur[k] = 0;
    for (int hop = 0; hop < 6; hop++) {
        int e = http_fetch(cur, r);
        for (k = 0; cur[k]; k++) r->url[k] = cur[k];
        r->url[k] = 0;
        if (e < 0) return e;
        if (r->status >= 300 && r->status < 400 && r->loc[0]) {
            char nx[256];
            if (url_resolve(cur, r->loc, nx, sizeof nx) < 0) return HE_URL;
            for (k = 0; nx[k]; k++) cur[k] = nx[k];
            cur[k] = 0;
            continue;
        }
        return 0;
    }
    return HE_LOOP;
}
/* ---- DNS через HTTPS (JSON API Cloudflare / Google) ---- */
static int json_status(const char *s, int n) {
    for (int i = 0; i + 8 < n; i++)
        if (s[i] == '"' && s[i + 1] == 'S' && s[i + 2] == 't' && s[i + 3] == 'a' && s[i + 4] == 't' && s[i + 5] == 'u' && s[i + 6] == 's' && s[i + 7] == '"') {
            int j = i + 8, v = 0;
            while (j < n && (s[j] == ' ' || s[j] == ':')) j++;
            if (j >= n || s[j] < '0' || s[j] > '9') return -1;
            while (j < n && s[j] >= '0' && s[j] <= '9') v = v * 10 + (s[j++] - '0');
            return v;
        }
    return -1;
}
static u32 json_first_ipv4(const char *s, int n) {          /* первый "data":"a.b.c.d" внутри "Answer" */
    int a = -1;
    for (int i = 0; i + 8 <= n; i++)
        if (s[i] == '"' && s[i + 1] == 'A' && s[i + 2] == 'n' && s[i + 3] == 's' && s[i + 4] == 'w' && s[i + 5] == 'e' && s[i + 6] == 'r' && s[i + 7] == '"') { a = i; break; }
    if (a < 0) return 0;
    for (int i = a; i + 6 < n; i++) {
        if (s[i] == '"' && s[i + 1] == 'd' && s[i + 2] == 'a' && s[i + 3] == 't' && s[i + 4] == 'a' && s[i + 5] == '"') {
            int j = i + 6;
            while (j < n && (s[j] == ' ' || s[j] == ':')) j++;
            if (j < n && s[j] == '"') {
                j++;
                char v[20]; int k = 0;
                while (j < n && s[j] != '"' && k < 19) v[k++] = s[j++];
                v[k] = 0;
                u32 ip;
                if (net_parse_ip(v, &ip)) return ip;          /* CNAME и AAAA отсекаются разбором IPv4 */
            }
        }
    }
    return 0;
}
static u32 http_doh_lookup(const char *name) {
#ifdef HTTP_TLS
    static const char *base[2] = { "https://8.8.8.8/resolve?type=A&name=", "https://1.1.1.1/dns-query?type=A&name=" };
    static const char *acc[2] = { "application/json", "application/dns-json" };
    static const u32 who[2] = { 0x08080808u, 0x01010101u };
    if (!http_doh_on || !http_raw || h_len(name) > 200) return 0;
    for (int k = 0; k < 2; k++) {
        char url[300]; int n = 0; url[0] = 0;
        h_apnd(url, &n, sizeof url, base[k]); h_apnd(url, &n, sizeof url, name);
        struct hres r;
        const char *old = http_accept;
        http_accept = acc[k];
        int e = http_fetch(url, &r);
        http_accept = old;
        doh_last_err = e;
        if (e < 0 || r.status != 200 || r.body_len <= 0) continue;
        const char *js = (const char *)http_buf + r.body_off;
        u32 ip = json_first_ipv4(js, r.body_len);
        if (ip) { dns_used = who[k]; return ip; }
        if (json_status(js, r.body_len) == 3) { dns_why = 4; return 0; }      /* и через HTTPS: такого имени нет */
    }
#else
    (void)name;
#endif
    return 0;
}
static u32 http_resolve(const char *name) {                  /* обычный DNS, затем DNS через HTTPS */
    dns_via_doh = 0;
    u32 ip = net_dns(name);
    if (ip) return ip;
    ip = http_doh_lookup(name);
    if (ip) { dns_via_doh = 1; dns_why = 0; }
    return ip;
}
#endif
