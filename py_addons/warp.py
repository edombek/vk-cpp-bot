# python-аддон /warp: случайное искажение геометрии приложенного изображения.
# Деформация делается средствами бота (img::warp на OpenCV): случайный
# перспективный перекос + плавное гауссово поле смещений («желе»).
# /warp <число> — повторяемый результат (одно зерно → одна деформация).

import vkcppbot

cmd = "/warp"
info = "случайно искажает геометрию картинки (можно /warp <зерно>)"
acess = 1
cat = "медиа"

def run(eventIn, eventOut, net, vk):
    seed = -1 # -1 — своя случайность на каждый вызов
    if len(eventIn.words) > 1:
        try:
            seed = int(eventIn.words[1])
        except ValueError:
            return "\nзерно должно быть числом: /warp 42"
    srcs = vkcppbot.img.sources(eventIn)
    if not srcs:
        return "\nне вижу картинки: приложи фото или картинку-файл к сообщению"
    newdocs = list(eventOut.docs)
    skipped = 0
    for d in srcs:
        try:
            im = vkcppbot.img.fromDoc(d, net)
        except ValueError:
            skipped += 1 # вложение не похоже на картинку
            continue
        art = im.warp(seed) # сила деформации — второй аргумент: warp(seed, 80)
        doc = vkcppbot.Doc()
        if doc.uploadPhoto("warp.png", art.toPng(), net, vk, eventIn.peer_id):
            newdocs.append(doc)
    if len(newdocs) > len(eventOut.docs):
        eventOut.docs = newdocs
        out = "\nготово — геометрия искажена"
        if seed >= 0:
            out += " (зерно " + str(seed) + ", повтори команду с тем же зерном — будет то же самое)"
        if skipped:
            out += " (пропущено не-картинок: " + str(skipped) + ")"
        return out
    # group-токен не может грузить фото в стену/комментарии (getMessagesUploadServer 901,
    # getWallUploadServer 27) — объясняем, как в C++ арт-командах
    if "wall" in str(eventIn.type):
        return "\nкартинку приложить не удалось (VK ограничивает загрузку фото от имени сообщества в стену) — пришли команду в личные сообщения"
    return "\nне удалось выгрузить результат (сбой upload-сервера) — попробуй ещё раз"
