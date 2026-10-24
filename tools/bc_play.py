#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Игра «Быки и коровы» с ботом через комментарии к посту стены.

Комментарии публикует токен самого сообщества с маркером «@@ » (бот принимает
свои комментарии только с ним — см. worker.cpp), ответ бота читается из его
stderr-лога (строка «└─> …» — печать ответа добавлена для живых проверок,
wall.getComments group-токеном недоступен).

Стратегия: держим множество кандидатов (4-значные без повторов, без ведущего 0),
первым ходом бьём 1234, дальше — кандидат, максимально разделяющий оставшиеся
группы ответов (minimax по размеру наибольшей группы).
"""
import itertools
import json
import os
import re
import sys
import time
import urllib.parse
import urllib.request

API = "https://api.vk.ru/method/"
VER = "5.131"
POST = 40581
MARK = "@@ "


def api(method, token, **params):
    data = urllib.parse.urlencode({"access_token": token, "v": VER, **params}).encode()
    req = urllib.request.Request(API + method, data=data)
    with urllib.request.urlopen(req, timeout=30) as r:
        resp = json.loads(r.read().decode("utf-8", "replace"))
    if "error" in resp:
        e = resp["error"]
        raise RuntimeError(f"{method}: {e.get('error_code')}: {e.get('error_msg')}")
    return resp.get("response")


def latest_log():
    import glob
    return max(glob.glob("/tmp/opencode/vkbot_run*.log"), key=os.path.getmtime)


def send_and_wait(token, log, cmd, offset, timeout=40.0):
    """Публикует комментарий-команду, ждёт строку ответа «└─> …» в логе.

    Возвращает (ответ, новый_offset). Ответ ищется после строки «-done!» нужной команды."""
    offset = os.path.getsize(log)
    api("wall.createComment", token, owner_id=-178228638, post_id=POST, message=MARK + cmd)
    deadline = time.time() + timeout
    buf = ""
    while time.time() < deadline:
        with open(log, encoding="utf-8", errors="replace") as f:
            f.seek(offset)
            buf += f.read()
            new_offset = f.tell()
        # ответ бота — строка «└─> …», идущая после «...done!» этой команды
        if "\n└─> " in buf:
            m = re.search(r"└─> (.*)$", buf, re.M)
            if m:
                return m.group(1).strip(), new_offset
        time.sleep(0.5)
    raise TimeoutError(f"нет ответа на «{cmd}» за {timeout}s")


def bc(bulls_cows):
    return bulls_cows


def score(guess, secret):
    b = sum(g == s for g, s in zip(guess, secret))
    c = sum(min(guess.count(d), secret.count(d)) for d in set(guess)) - b
    return (b, c)


def main():
    cfg = json.load(open("config.json", encoding="utf-8"))
    token = cfg["token"]
    log = latest_log()
    print(f"лог: {log}, пост {POST}")

    start, off = send_and_wait(token, log, "/bc", 0)
    print("старт:", start[:60])

    digits = "0123456789"
    cands = ["".join(p) for p in itertools.permutations(digits, 4) if p[0] != "0"]
    guess = "1234"
    for turn in range(1, 100):
        answer, off = send_and_wait(token, log, f"/bc {guess}", off)
        print(f"ход {turn}: {guess} -> {answer}")
        if "отгадал" in answer:
            print(f"🎉 отгадано за {turn} ходов: {guess}")
            return 0
        mb = re.search(r"Быков:\s*(\d+)", answer)
        mc = re.search(r"Коров:\s*(\d+)", answer)
        if not mb or not mc:
            print("не понял ответ, стоп"); return 1
        b, c = int(mb.group(1)), int(mc.group(1))
        cands = [x for x in cands if score(guess, x) == (b, c)]
        if not cands:
            print("кандидатов не осталось — противоречие"); return 1
        print(f"   кандидатов: {len(cands)}")
        # следующий ход: разделяющий (minimax по наибольшей группе)
        if len(cands) == 1:
            guess = cands[0]
        else:
            pool = cands if len(cands) <= 40 else cands[::max(1, len(cands) // 40)]
            best, best_key = pool[0], None
            for g in pool:
                groups = {}
                for x in cands:
                    groups.setdefault(score(g, x), 0)
                    groups[score(g, x)] += 1
                key = max(groups.values())
                if best_key is None or key < best_key:
                    best, best_key = g, key
            guess = best
    print("не отгадал за отведённое число ходов")
    return 1


if __name__ == "__main__":
    sys.exit(main())
