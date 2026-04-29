project('chatapp', 'c')

hackode_dep = dependency('hackode', required: true)

executable(
  'chat-server',
  'chat_server.c',
  dependencies: [hackode_dep],
  install: false,
)

executable(
  'chat-client',
  'chat_client.c',
  dependencies: [hackode_dep],
  install: false,
)
