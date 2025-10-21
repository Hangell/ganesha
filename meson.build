project('ganesha', 'c',
  version: '0.1.0',
  default_options: ['warning_level=2', 'c_std=c11']
)

gtkdep   = dependency('gtk4')
adwdep   = dependency('libadwaita-1')
soupdep  = dependency('libsoup-3.0')
jsondep  = dependency('json-glib-1.0')
srcdep   = dependency('gtksourceview-5')   # <-- novo

executable('ganesha',
  sources: ['main.c'],
  dependencies: [gtkdep, adwdep, soupdep, jsondep, srcdep]
)
