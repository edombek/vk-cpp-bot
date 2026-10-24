#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Получение user-токена ВКонтакте и сохранение его в config.json (ключ user_token).

Токен пользователя нужен боту для /video и аудиосообщений, а также для
автоматических живых проверок (писать сообществу от имени пользователя).

Как получить client_id/secret: https://vk.com/apps?act=manage — «Создать приложение»
(или https://vk.com/editapp?act=create), тип «веб-сайт»,
в «Адрес сайта / Authorized redirect URI » указать http://localhost:8765/callback,
права: сообщения (messages). секрет показывается один раз.

Режимы:
  ./tools/vk_user_token.py            — auth code flow: откроется браузер,
                                        скрипт поднимет http://localhost:8765
  ./tools/vk_user_token.py --paste    — implicit flow: открыть ссылку, скопировать
                                        адрес итоговой страницы (или сам токен)
  ./tools/vk_user_token.py --check    — проверить/показать текущий user_token из config.json

client_id/secret берутся из аргументов, окружения (VK_CLIENT_ID/VK_CLIENT_SECRET)
или config.json (vk_client_id/vk_client_secret); после успешного запуска они
сохраняются в config.json, чтобы в следующий раз запускать без параметров.
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
API = "https://api.vk.com/method/"
OAUTH = "https://oauth.vk.com/"
REDIRECT = "http://localhost:8765/callback"
SCOPES = "messages,friends,status"  # friends/status — чтобы проверки профиля работали шире


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

def paste_flow(client_id):
    url = (OAUTH + "authorize?client_id=" + client_id +
           "&redirect_uri=https%3A%2F%2Foauth.vk.com%2Fblank.html" +
           "&scope=" + SCOPES + "&response_type=token&display=page&v=5.131")
    print("Откройте в браузере и разрешите доступ:")
    print(url)
    try:
        webbrowser.open(url)
    except Exception:
        pass
    print("После разрешения браузер уйдёт на oauth.vk.com/blank.html#access_token=...")
    print("Скопируйте адрес страницы целиком (или сам access_token) и вставьте:")
    s = sys.stdin.readline().strip()
    if "#" in s:
        s = urllib.parse.urlparse(s).fragment or s.split("#", 1)[1]
    q = urllib.parse.parse_qs(s)
    tok = q.get("access_token", [s])[0]
    return tok or None


def main():
    ap = argparse.ArgumentParser(description="получить user-токен VK в config.json")
    ap.add_argument("--check", action="store_true", help="проверить user_token из config.json")
    ap.add_argument("--paste", action="store_true", help="implicit flow (вставить токен руками)")
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

    client_id = a.client_id or os.environ.get("VK_CLIENT_ID") or cfg.get("vk_client_id")
    if not client_id:
        try:
            client_id = input("client_id приложения VK: ").strip()
        except EOFError:
            sys.exit("нужен client_id")

    if a.paste:
        tok = paste_flow(client_id)
    else:
        secret = a.client_secret or os.environ.get("VK_CLIENT_SECRET") or cfg.get("vk_client_secret")
        if not secret:
            try:
                secret = input("client_secret приложения VK: ").strip()
            except EOFError:
                sys.exit("нужен client_secret")
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
