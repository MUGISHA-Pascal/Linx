# Linx Chat

A simple yet powerful command-line chat application with private messaging and emoji support, built with C and socket programming.

## Features

- **Multi-user chat** - Chat with multiple users simultaneously
- **Private Messaging** - Send direct messages to specific users
- **Emoji Support** - Express yourself with emoji shortcuts
- **Colorful Interface** - Easy-to-read colored terminal output
- **Command System** - Built-in commands for better control
- **Cross-Platform** - Works on any system with a C compiler

## Prerequisites

- GCC compiler
- POSIX-compliant system (Linux, macOS, WSL)
- Basic terminal knowledge

## Installation

1. Clone the repository:

   ```bash
   git clone https://github.com/MUGISHA-Pascal/Linx.git
   cd Linx
   ```

2. Compile the server and client:
   ```bash
   make
   ```
   Or compile manually:
   ```bash
   gcc server.c -o server -pthread
   gcc client.c -o client -pthread
   ```

## Usage

### Starting the Server

```bash
./server
```

The server will start and display its IP address. Other users will need this IP to connect.

### Connecting Clients

On the same machine:

```bash
./client 127.0.0.1
```

On a different machine (replace with server's IP):

```bash
./client 192.168.1.100
```

### Available Commands

- `/help` - Show all available commands
- `/exit` - Leave the chat
- `/clear` - Clear the screen
- `/users` or `/list` - Show all connected users
- `/msg <username> <message>` - Send a private message
- `/nick <new_username>` - Change your username

### Emoji Shortcuts

```
:)  😊  :(  😞  :D  😃  ;)  😉
:P  😛  :O  😮  :/  😕  <3  ❤️
lol 😂  rofl 🤣  wink 😉  cool 😎
```

## Example Session

```
=== Welcome to Linx Chat ===
Type /help for a list of commands

[alice] Hello everyone! 😊
[bob] Hi Alice! How are you? 😃
[alice] /msg bob I'm great, thanks! 😊
[PM from alice] I'm great, thanks! 😊
```

## Contributing

Feel free to submit issues and enhancement requests. Pull requests are welcome!

## License

This project is open source and available under the [MIT License](LICENSE).
