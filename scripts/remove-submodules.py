import json

with open('compile_commands.json', 'r') as f:
    commands = json.load(f)
    commands = [command for command in commands
                if 'submodules' not in command['file']]
with open('compile_commands.json', 'w') as f:
    json.dump(commands, f)
