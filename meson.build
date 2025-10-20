project('ganesha', 'c',
  version: '0.1.0',
  default_options: ['warning_level=2', 'c_std=c11']
)

gtkdep = dependency('gtk4')
adwdep = dependency('libadwaita-1')

executable('ganesha',
  sources: ['main.c'],
  dependencies: [gtkdep, adwdep]
)
