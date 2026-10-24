#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Живые проверки бота через КОММЕНТАРИИ к одной записи стены (не спамит стену).

Схема: на стене сообщества есть один тестовый пост (по умолчанию id 40579).
Каждая проверяемая команда публикуется комментарием под ним — токеном ДРУГОЙ
страницы (config.json["checker_token"]) или токеном самого сообщества (--self,
с маркером «@@ »). Ответ бота — комментарий в ветку команды (wall.createComment
+ reply_to_cid).

Результат определяется по stderr-логу бота: строки вида
    /stat vk.com/id-90697853 (-90697853/-178228638(wall_reply_new))
    /stat vk.com/id-90697853 (-90697853/-178228638(wall_reply_new))-done!
означают приём и успешное выполнение; «WORK ERROR»/«VK ERROR» между ними — провал.

Удалить комментарии group-токеном нельзя (wall.deleteComment — ошибка 27,
comments.* выведены из API) — по окончании печатаются ссылки для ручной чистки.

2026-10-24: VK закрыл group-токенам доступ к чужим стенам (wall.createComment в
чужое сообщество — ошибка 15), поэтому для автоматизации есть режим --self:
комментарии публикуются токеном самого сообщества с маркером «@@ », который
бот разрешает только для своих комментариев (ответы маркера не содержат —
петля исключена).

Примеры:
    ./tools/live_check.py --self /stat /help
    ./tools/live_check.py --post 40579 --timeout 30 "/apoddate 01.10.2014" /apodrand
"""

import argparse
import json
import os
import re
import sys
import time
import urllib.parse
import urllib.request

API = "https://api.vk.ru/method/"
VER = "5.131"


def api(method, token, **params):
    data = urllib.parse.urlencode({"access_token": token, "v": VER, **params}).encode()
    req = urllib.request.Request(API + method, data=data)
    with urllib.request.urlopen(req, timeout=30) as r:
        resp = json.loads(r.read().decode("utf-8", "replace"))
    if "error" in resp:
        e = resp["error"]
        raise RuntimeError(f"{method}: ошибка {e.get('error_code')}: {e.get('error_msg')}")
    return resp.get("response")


def find_log(explicit):
    if explicit:
        return explicit
    import glob
    logs = glob.glob("/tmp/opencode/vkbot_run*.log")
    if not logs:
        sys.exit("Не найден лог бота (/tmp/opencode/vkbot_run*.log) — укажите --log")
    return max(logs, key=os.path.getmtime)


def main():
    p = argparse.ArgumentParser(description="Проверка команд бота комментариями под одним постом")
    p.add_argument("commands", nargs="+", help="команды, например /stat \"/apoddate 01.10.2014\"")
    p.add_argument("--config", default="config.json", help="путь к config.json (token + checker_token)")
    p.add_argument("--post", type=int, default=40579, help="id записи стены (default 40579)")
    p.add_argument("--log", help="лог бота (default — свежайший /tmp/opencode/vkbot_run*.log)")
    p.add_argument("--timeout", type=float, default=40.0, help="секунд ожидания -done! на команду")
    p.add_argument("--self", action="store_true",
                   help="самопроверка: комментарии токеном самого сообщества с маркером «@@ » "
                        "(bot их обрабатывает; нужен только token сообщества)")
    a = p.parse_args()

    cfg = json.load(open(a.config, encoding="utf-8"))
    bot_token = cfg.get("token")
    chk_token = cfg.get("checker_token")
    if not bot_token:
        sys.exit("В config.json нет token (сообщество бота)")
    if a.self:
        chk_token = bot_token  # свои комментарии бот принимает только с маркером «@@ »
    elif not chk_token:
        sys.exit("В config.json нет checker_token — токен ДРУГОЙ страницы/сообщества, "
                 "от имени которой будут публиковаться комментарии-команды "
                 "(или запустите с --self — самопроверка токеном сообщества)")
    elif chk_token == bot_token:
        sys.exit("checker_token совпадает с token сообщества бота — для таких проверок "
                 "запустите с --self (бот принимает свои комментарии с маркером «@@ »)")

    log_path = find_log(a.log)
    bot_id = api("groups.getById", bot_token)[0]["id"]
    chk_id = api("groups.getById", chk_token)[0]["id"]
    owner = -bot_id
    print(f"Лог: {log_path}\nСтена: -{bot_id}, пост {a.post}; проверяющий: -{chk_id}\n")

    results = []
    comment_ids = []
    for cmd in a.commands:
        offset = os.path.getsize(log_path)
        try:
            cid = api("wall.createComment", chk_token, owner_id=owner, post_id=a.post,
                      message=("@@ " if a.self else "") + cmd)
        except RuntimeError as e:
            print(f"[{cmd}] НЕ ОПУБЛИКОВАН комментарий: {e}")
            results.append((cmd, "NO-POST"))
            continue
        if isinstance(cid, dict):  # на случай нового формата ответа
            cid = cid.get("comment_id") or cid.get("id")
        comment_ids.append(cid)
        word = cmd.split()[0].lower()
        recv_re = re.compile(re.escape(word) + r" vk\.com/id\S* \(" + str(chk_id) + r"/" + str(owner) + r"\(wall_reply_new\)\)\s*$")
        done_re = re.compile(re.escape(word) + r" vk\.com/id\S* \(" + str(chk_id) + r"/" + str(owner) + r"\(wall_reply_new\)\)-done!\s*$")
        err_re = re.compile(r"WORK ERROR|VK ERROR|FATAL")

        deadline = time.time() + a.timeout
        buf = ""
        status, detail = "TIMEOUT", ""
        while time.time() < deadline:
            with open(log_path, encoding="utf-8", errors="replace") as f:
                f.seek(offset)
                buf += f.read()
                offset = f.tell()
            lines = buf.splitlines()
            got_done = any(done_re.search(l) for l in lines)
            got_recv = any(recv_re.search(l) for l in lines)
            errline = next((l for l in lines if err_re.search(l)), None)
            if got_done:
                status, detail = "PASS", ""
                break
            if errline:
                status, detail = "FAIL", errline.strip()
                break
            if got_recv:
                status = "RECV"  # принято, выполнения нет
            time.sleep(0.5)
        mark = {"PASS": "✅", "FAIL": "❌", "RECV": "⏳", "TIMEOUT": "⌛"}[status]
        extra = f"  ({detail})" if detail else ("" if status == "PASS" else "  (нет строки -done!)")
        print(f"{mark} [{cmd}] {status}{extra}")
        results.append((cmd, status))

    print("\nСсылки на комментарии-команды (бот не может их удалить — чистить вручную):")
    for cid in comment_ids:
        print(f"  https://vk.com/wall-{bot_id}_{a.post}?cid={cid}")
    bad = [c for c, s in results if s != "PASS"]
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
