# python-аддон /space: астрофото NASA (Astronomy Picture of the Day) — берёт
# случайный пост из последних 20 на science.nasa.gov (без ключей) и прикладывает
# картинку. Источник тот же, что у C++ /apod: legacy api.nasa.gov архивируют.

import json
import random
import re
import vkcppbot

cmd = "/space"
info = "астрофото NASA (случайное из последних) с картинкой"
acess = 1
cat = "python"

def run(eventIn, eventOut, net, vk):
    resp = net.send("https://science.nasa.gov/wp-json/wp/v2/apod-basic?per_page=20")
    try:
        posts = json.loads(resp)
    except ValueError:
        return "NASA APOD сейчас недоступен (ответ не JSON)"
    if not isinstance(posts, list) or not posts:
        return "NASA APOD: в ответе пусто"
    post = random.choice(posts)
    title = post.get("title", "APOD")
    out = "\n" + str(title)
    if post.get("date"):
        out += " (" + str(post["date"])[:10] + ")"
    expl = str(post.get("explanation", ""))
    if expl:
        expl = re.sub(r"<[^>]+>", "", expl).strip() # WordPress присылает explanation с HTML-тегами
        out += ":\n\n" + expl[:600]
    imgurl = post.get("hdurl") or ""
    if not imgurl:
        if post.get("permalink"):
            return out + "\n\nэто не картинка (видео/без hdurl), первоисточник:\n" + str(post["permalink"])
        return out
    data = net.send(imgurl) # бинарный ответ net.send отдаёт как bytes
    if not isinstance(data, bytes):
        return out + "\n\nпо ссылке не картинка: " + imgurl
    art = vkcppbot.Doc()
    if art.uploadPhoto("space.jpg", data, net, vk, eventIn.peer_id):
        eventOut.docs = list(eventOut.docs) + [art]
        return out + "\n\nкартинка приложена"
    # для group-токена VK не даёт грузить фото в стену/комментарии (901/27) — объясняем
    return out + "\n\nкартинку приложить не удалось (VK ограничивает загрузку фото от имени сообщества в стену) — пришли команду в личные сообщения"
