#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Получение user-токена ВКонтакте и сохранение его в config.json (ключ user_token).

Токен пользователя нужен боту для /video и аудиосообщений, а также для
автоматических живых проверок (писать сообществу от имени пользователя).

По умолчанию — публичный client_id официального клиента vk.com (2671, так же
работает библиотека vk_api) и редирект на oauth.vk.com/blank.html:
регистрировать своё приложение НЕ нужно (в VK ID «Моё приложение» localhost
не принимают — нужен https-домен, он нам не обязателен). Свой client_id —
--client-id/--client-secret (приложение: https://vk.com/apps?act=manage).

Режимы:
  ./tools/vk_user_token.py --direct     — прямая авторизация desktop-клиента
                                          (телефон+пароль, при запросе — код 2FA
                                          и капча; пароль только в терминале)
  ./tools/vk_user_token.py --print-url  — напечатать ссылку авторизации: открыть,
                                          разрешить, скопировать адрес итоговой
                                          страницы (blank.html#access_token=...)
  ./tools/vk_user_token.py --save '<адрес итоговой страницы или сам токен>'
                                        — провалидировать и сохранить в config.json
  ./tools/vk_user_token.py --paste      — интерактивно: ссылка + вставка с клавиатуры
  ./tools/vk_user_token.py              — auth code flow через http://localhost:8765
                                          (для своего приложения)
  ./tools/vk_user_token.py --check      — проверить/показать текущий user_token

Замечание 2026: implicit-токены сторонних/закрытых клиентов (Kate Mobile 2685278,
VK Admin 6121396) VK отвергает (error 9 / «application is blocked») — рабочий путь
для scope messages: --direct (desktop-клиент 2671, как в библиотеке vk_api).
"""

import argparse
import http.server
import json
import os
import secrets
import sys
import threading
import time
import urllib.parse
import urllib.request
import webbrowser

CONFIG = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "config.json")
API = "https://api.vk.ru/method/"
OAUTH = "https://oauth.vk.ru/"
REDIRECT = "http://localhost:8765/callback"
SCOPES = "messages,friends,status"  # friends/status — чтобы проверки профиля работали шире
# implicit-поток: публичный client_id живого approved-клиента — 6222115 (дефолт vk_api
# 11.x, 2026).Kate Mobile 2685278/VK Admin 6121396 в 2026 мёртвы (error 9/blocked),
# grant_type=password VK тоже закрыл (vk_api перешёл на oauth.vk.ru/authorize).
# scope числом: friends(2^1)+status(2^10)+messages(2^12)+offline(2^16) = 70658
DEFAULT_CLIENT = "6222115"
SCOPE_NUM = 70658


def load_config():
    try:
        with open(CONFIG, encoding="utf-8") as f:
            return json.load(f)
    except FileNotFoundError:
        return {}


def save_config(cfg):
    tmp = CONFIG + ".safe"
    with open(tmp, "w", encoding="utf-8") as f:
        json.dump(cfg, f, ensure_ascii=False, indent=4)
    os.replace(tmp, CONFIG)  # атомарно, как fs::safeWriteData в самом боте


def vk_call(method, token, params=None):
    q = {"access_token": token, "v": "5.131"}
    q.update(params or {})
    req = urllib.request.Request(
        API + method,
        data=urllib.parse.urlencode(q).encode(),
        headers={"User-Agent": "vk-cpp-bot-token-helper"},
    )
    with urllib.request.urlopen(req, timeout=30) as r:
        return json.loads(r.read().decode())


def check_token(token):
    r = vk_call("users.get", token, {"fields": "bdate"})
    if "error" in r:
        e = r["error"]
        print(f"токен не работает: ошибка {e.get('error_code')} — {e.get('error_msg')}")
        if e.get("error_code") in (5, 6, 9):  # authorization failed / token expired / outdated
            print("нужно получить новый токен")
        return None
    u = r["response"][0]
    print(f"токен пользователя: id {u['id']}, {u.get('first_name')} {u.get('last_name')}"
          + (f", д.р. {u['bdate']}" if u.get("bdate") else ""))
    return u


# ---------- режим 1: auth code flow через локальный сервер ----------

class _CallbackHandler(http.server.BaseHTTPRequestHandler):
    result = {}

    def do_GET(self):
        q = urllib.parse.parse_qs(urllib.parse.urlparse(self.path).query)
        _CallbackHandler.result = {k: v[0] for k, v in q.items()}
        self.send_response(200)
        self.send_header("Content-Type", "text/html; charset=utf-8")
        self.end_headers()
        ok = "code" in _CallbackHandler.result
        self.wfile.write("<h2>vkbk: ".encode("utf-8") +
                         ("получили code — можно закрывать вкладку</h2>".encode("utf-8") if ok
                          else "ошибка авторизации, смотрите консоль</h2>".encode("utf-8")))

    def log_message(self, *a):  # тихонько
        pass


def auth_code_flow(client_id, client_secret):
    state = secrets.token_hex(8)
    url = (OAUTH + "authorize?client_id=" + client_id +
           "&redirect_uri=" + urllib.parse.quote(REDIRECT, safe="") +
           "&scope=" + SCOPES + "&response_type=code&state=" + state + "&v=5.131")
    srv = http.server.HTTPServer(("127.0.0.1", 8765), _CallbackHandler)
    threading.Thread(target=srv.serve_forever, daemon=True).start()
    print("Откройте в браузере (сейчас попробуем сами):")
    print(url)
    try:
        webbrowser.open(url)
    except Exception:
        pass
    print("Ждём возврат на http://localhost:8765/callback ...")
    for _ in range(600):  # 10 минут
        if _CallbackHandler.result:
            break
        time.sleep(0.5)
    srv.shutdown()
    res = _CallbackHandler.result
    if not res:
        print("не дождались колбэка")
        return None
    if "error" in res:
        print(f"отказ авторизации: {res.get('error')} — {res.get('error_description')}")
        return None
    if res.get("state") != state:
        print("state не совпал — возможно CSRF, отменяем")
        return None
    data = urllib.parse.urlencode({
        "client_id": client_id, "client_secret": client_secret,
        "redirect_uri": REDIRECT, "code": res["code"],
    }).encode()
    req = urllib.request.Request(OAUTH + "access_token", data=data)
    try:
        with urllib.request.urlopen(req, timeout=30) as r:
            j = json.loads(r.read().decode())
    except urllib.error.HTTPError as e:
        j = json.loads(e.read().decode())
    if "access_token" not in j:
        print(f"обмен code на токен не удался: {j.get('error')} — {j.get('error_description')}")
        return None
    exp = j.get("expires_in")
    print("получили токен" + (f", срок действия {int(exp)/3600:.0f} ч" if exp else " (бессрочный)"))
    return j["access_token"]


# ---------- режим 2: implicit (response_type=token), вставка руками ----------

def authorize_url(client_id):
    return (OAUTH + "authorize?client_id=" + client_id +
            "&redirect_uri=https%3A%2F%2Foauth.vk.ru%2Fblank.html" +
            "&scope=" + str(SCOPE_NUM) + "&response_type=token&display=page&v=5.131")


def extract_token(s):
    """из адреса blank.html#access_token=..., из query или самого токена"""
    if "#" in s:
        s = urllib.parse.urlparse(s).fragment or s.split("#", 1)[1]
    q = urllib.parse.parse_qs(s)
    return (q.get("access_token", [s])[0] or "").strip()


def paste_flow(client_id):
    url = authorize_url(client_id)
    print("Откройте в браузере и разрешите доступ:")
    print(url)
    try:
        webbrowser.open(url)
    except Exception:
        pass
    print("После разрешения браузер уйдёт на oauth.vk.com/blank.html#access_token=...")
    print("Скопируйте адрес страницы целиком (или сам access_token) и вставьте:")
    s = sys.stdin.readline().strip()
    return extract_token(s)


def direct_flow():
    """Получение токена через vk_api (веб-логин login.php + выдача токена из
    доверенной сессии) — единственная схема, живая в 2026: прямые oauth-клиенты
    VK блокирует («application is blocked»). Пароль через getpass."""
    try:
        import getpass
        import vk_api
        from vk_api.exceptions import ApiCaptchaRequired
    except ImportError:
        return _direct_flow_stdlib()
    login = input("телефон аккаунта (цифрами, 7...): ").strip()
    password = getpass.getpass("пароль от аккаунта VK: ")
    def auth_handler():
        print("VK просит подтверждение входа (новое устройство/2FA)")
        code = input("код из SMS/push: ").strip()
        return code, True
    def captcha_handler(captcha):
        path = "/tmp/opencode/vk_captcha.png"
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "wb") as f:
            f.write(captcha.get_image())
        print(f"нужна капча — картинка сохранена: {path}")
        return captcha.sid, input("код с картинки: ").strip()
    try:
        session = vk_api.VkApi(login, password, scope=SCOPE_NUM, api_version="5.131",
                               auth_handler=auth_handler, captcha_handler=captcha_handler)
        session.auth(reauth=True)
    except vk_api.AuthError as e:
        print(f"vk_api: {e}")
        return None
    tok = (session.token or {}).get("access_token")
    if tok:
        print("получили токен")
    return tok


def _direct_flow_stdlib():
    """запасной путь без vk_api (grant_type=password) — в 2026 VK его закрыл,
    оставлен на случай возврата"""
    import getpass
    login = input("телефон аккаунта (цифрами, 7...): ").strip()
    password = getpass.getpass("пароль от аккаунта VK: ")
    params = {"grant_type": "password", "client_id": "2671", "client_secret": "w0x7ohD9PQOkRS7cR43G",
              "username": login, "password": password, "scope": str(SCOPE_NUM), "v": "5.131",
              "2fa_supported": "1", "force_2fa": "0"}
    code = None
    for _ in range(8):
        p = dict(params)
        if code:
            p["code"] = code
        req = urllib.request.Request("https://oauth.vk.com/token",
                                     data=urllib.parse.urlencode(p).encode(),
                                     headers={"User-Agent": "VKChat/8.90"})
        try:
            with urllib.request.urlopen(req, timeout=30) as r:
                j = json.loads(r.read().decode())
        except urllib.error.HTTPError as e:
            try:
                j = json.loads(e.read().decode())
            except ValueError:
                print("VK вернул не-JSON"); return None
        err = j.get("error")
        if not err:
            print("получили токен" + (f", срок действия {int(j['expires_in'])/3600:.0f} ч" if j.get("expires_in") else ""))
            return j["access_token"]
        if err in ("two_factor_required", "two_factor_authorized_error"):
            if j.get("phone_mask"):
                print(f"VK просит код 2FA на {j['phone_mask']}")
            if j.get("password"):
                params["password"] = j["password"]  # одноразовый токен вместо пароля
            code = input("код 2FA: ").strip()
            continue
        if err == "need_validation":
            path = "/tmp/opencode/vk_captcha.png"
            os.makedirs(os.path.dirname(path), exist_ok=True)
            urllib.request.urlretrieve(j.get("captcha_img", ""), path)
            print(f"нужна капча — картинка сохранена: {path}")
            params["captcha_sid"] = j.get("captcha_sid")
            params["captcha_key"] = input("код с картинки: ").strip()
            continue
        if err == "need_confirmation":
            print("VK требует подтвердить новое устройство:")
            print(j.get("error_description", j))
            code = input("код подтверждения (Enter — отмена): ").strip()
            if not code:
                return None
            continue
        print(f"ошибка: {err} — {j.get('error_description', j.get('error_msg', ''))}")
        if err in ("invalid_client", "unsupported_grant_type"):
            print("клиент 2671, похоже, тоже заблокирован — сообщите, попробуем другие публичные креды")
        return None
    print("слишком много попыток — попробуй ещё раз")
    return None


def main():
    ap = argparse.ArgumentParser(description="получить user-токен VK в config.json")
    ap.add_argument("--check", action="store_true", help="проверить user_token из config.json")
    ap.add_argument("--paste", action="store_true", help="implicit flow (вставить токен руками)")
    ap.add_argument("--direct", action="store_true", help="прямая авторизация desktop-клиента (телефон+пароль)")
    ap.add_argument("--print-url", action="store_true", help="только напечатать ссылку авторизации")
    ap.add_argument("--save", metavar="ССЫЛКА_ИЛИ_ТОКЕН", help="сохранить токен из адреса blank.html/токена")
    ap.add_argument("--client-id", default=None)
    ap.add_argument("--client-secret", default=None)
    a = ap.parse_args()

    cfg = load_config()
    if a.check:
        tok = cfg.get("user_token")
        if not tok:
            print("user_token в config.json нет")
            sys.exit(1)
        sys.exit(0 if check_token(tok) else 1)

    if a.save:
        tok = extract_token(a.save)
        if not tok:
            sys.exit("токен не распознан во вставленной строке")
        if not check_token(tok):
            sys.exit("токен не проходит users.get — в config не пишем")
        cfg["user_token"] = tok
        save_config(cfg)
        print(f"сохранено: {CONFIG} (ключ user_token)")
        print("не забудьте перезапустить бота, чтобы он подхватил user_token")
        return

    if a.direct:
        tok = direct_flow()
        if not tok:
            sys.exit("токен не получен")
        if not check_token(tok):
            sys.exit("токен не проходит users.get — в config не пишем")
        cfg["user_token"] = tok
        save_config(cfg)
        print(f"сохранено: {CONFIG} (ключ user_token)")
        print("не забудьте перезапустить бота, чтобы он подхватил user_token")
        return

    # Публичные client_id официальных/одобренных клиентов (implicit не требует секрета)
    client_id = a.client_id or os.environ.get("VK_CLIENT_ID") or cfg.get("vk_client_id") or DEFAULT_CLIENT
    if client_id == DEFAULT_CLIENT:
        print(f"(client_id {DEFAULT_CLIENT} — одобренный десктоп-клиент VK, своё приложение не требуется)")

    if a.print_url:
        print(authorize_url(client_id))
        return

    if a.paste:
        tok = paste_flow(client_id)
    else:
        secret = a.client_secret or os.environ.get("VK_CLIENT_SECRET") or cfg.get("vk_client_secret")
        if not secret:
            try:
                secret = input("client_secret приложения VK: ").strip()
            except EOFError:
                sys.exit("нужен client_secret (или используй --print-url + --save)")
        tok = auth_code_flow(client_id, secret)
        if secret:
            cfg["vk_client_secret"] = secret
    if not tok:
        sys.exit("токен не получен")

    cfg["vk_client_id"] = client_id
    if not check_token(tok):
        sys.exit("полученный токен не проходит users.get — в config не пишем")
    cfg["user_token"] = tok
    save_config(cfg)
    print(f"сохранено: {CONFIG} (ключ user_token)")
    print("не забудьте перезапустить бота, чтобы он подхватил user_token")


if __name__ == "__main__":
    main()
