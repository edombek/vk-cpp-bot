# python-аддон /echo: показывает содержимое запроса — тип события, id, текст,
# вложения (docs), пересланные сообщения (fwds). Демонстрация минимального аддона.

cmd = "/echo"
info = "показать содержимое запроса (вложения, пересылки, id)"
acess = 1
cat = "python"

def run(eventIn, eventOut, net, vk):
    lines = [
        "тип события: " + str(eventIn.type),
        "id сообщения: " + str(eventIn.id) + ", от: " + str(eventIn.from_id) + ", диалог: " + str(eventIn.peer_id),
        "чат: " + ("да" if eventIn.is_chat else "нет") + ", слов: " + str(len(eventIn.words)),
        "текст: " + (eventIn.msg if eventIn.msg else "(пусто)"),
    ]
    if eventIn.docs:
        lines.append("вложения (" + str(len(eventIn.docs)) + "):")
        for d in eventIn.docs:
            name = str(d.ext) if d.ext else str(d.type)
            url = str(d.url).split("?")[0] if d.url else "нет ссылки"
            lines.append("  " + str(d.type) + " «" + name + "» " + url)
    if eventIn.fwds:
        lines.append("пересланные сообщения (" + str(len(eventIn.fwds)) + "):")
        for f in eventIn.fwds:
            lines.append("  от " + str(f.from_id) + ": " + (f.msg[:100] if f.msg else "(без текста)"))
    if not eventIn.docs and not eventIn.fwds:
        lines.append("вложений и пересылок нет")
    return "\n" + "\n".join(lines)
