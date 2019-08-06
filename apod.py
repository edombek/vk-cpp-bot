#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Created on Thu Jun 27 16:01:46 2019

@author: edombek
"""
#скрипт для постиинга, прописать на автозапус раз в день)


import json
conf = json.loads(open('config.json').read())

import vk_api

#получаем ид группы от имени группы
vk_session = vk_api.VkApi(token = conf['token'])
vk = vk_session.get_api()
group_id = vk.groups.getById()[0]['id']

#логин от имени админа группы
vk_session = vk_api.VkApi(token = conf['user_token'])
vk = vk_session.get_api()

#nasa apod
import urllib.request
resp = json.loads(urllib.request.urlopen('https://api.nasa.gov/planetary/apod?api_key=DEMO_KEY').read())
urllib.request.urlretrieve(resp['hdurl'], 'temp.jpg') 
upload = vk_api.VkUpload(vk_session)
photo = upload.photo_wall(photos='temp.jpg', group_id = group_id)
photo = 'photo{}_{}_{}'.format(photo[0]['owner_id'], photo[0]['id'], photo[0]['access_key'])
print(resp)
try:
	resp['copyright']
except:
	resp['copyright'] = None
text = 'Astronomy Picture of the Day (apod.nasa.gov)\n\n{}\n\n{}\n\n©{}'.format(resp['title'], resp['explanation'], resp['copyright'])

#постим
vk.wall.post(message=text, owner_id = -group_id, from_group = 1, attachments = photo)
