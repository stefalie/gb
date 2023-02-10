import json  # TODO: rm
import requests

url = "https://web.archive.org/web/20220912165518/https://gbdev.io/gb-opcodes/Opcodes.json"
url = "https://gbdev.io/gb-opcodes/Opcodes.json"
response = requests.get(url)
assert response.status_code == 200
instructions = response.json()

print(json.dumps(instructions["unprefixed"]["0x00"], indent=4))

