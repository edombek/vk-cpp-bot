# python-аддон /invert: обработка приложенного к сообщению изображения —
# инверсия цветов. Берёт картинку из вложений (фото/файл), без вложения — фото
# профиля автора (vkcppbot.img.sources), фильтрует средствами бота (OpenCV)
# и прикладывает результат. Демонстрация Image API и uploadPhoto.

import vkcppbot

cmd = "/invert"
info = "инверсия цветов картинки (вложение или фото профиля)"
acess = 1
cat = "python"

def run(eventIn, eventOut, net, vk):
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
        art = im.invert() # есть и другие фильтры: pix(), crt(), neon(), gray(), scale(w, h)
        doc = vkcppbot.Doc()
        if doc.uploadPhoto("invert.png", art.toPng(), net, vk, eventIn.peer_id):
            newdocs.append(doc)
    if len(newdocs) > len(eventOut.docs):
        eventOut.docs = newdocs
        out = "\nготово — инверсия приложена"
        if skipped:
            out += " (пропущено не-картинок: " + str(skipped) + ")"
        return out
    # group-токен не может грузить фото в стену/комментарии (getMessagesUploadServer 901,
    # getWallUploadServer 27) — объясняем, как в C++ арт-командах
    if "wall" in str(eventIn.type):
        return "\nкартинку приложить не удалось (VK ограничивает загрузку фото от имени сообщества в стену) — пришли команду в личные сообщения"
    return "\nне удалось выгрузить результат (сбой upload-сервера) — попробуй ещё раз"
