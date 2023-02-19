import requests

url='https://zenquotes.io/api/quotes'

reply = requests.get(url)
data = reply.json()

with open('motd.txt', 'w') as f:
    for element in data:
        quote = element['q']
        f.write(f"{quote}\n")       
    print('success')


