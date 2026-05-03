# Define the project name and language (C)
project('gst-zstddec', 'c')

# Declare external dependencies using pkg-config
gst_dep = dependency('gstreamer-1.0')
gstbase_dep = dependency('gstreamer-base-1.0')
zstd_dep = dependency('libzstd')

# Generate a config.h header with project metadata
# These values are referenced by GST_PLUGIN_DEFINE() in the C source
# when the source file includes config.h via #ifdef HAVE_CONFIG_H.
cdata = configuration_data()
cdata.set_quoted('PACKAGE_VERSION', '1.0.0')
cdata.set_quoted('GST_LICENSE', 'LGPL')
cdata.set_quoted('GST_PACKAGE_NAME', 'GStreamer zstd decoder')
cdata.set_quoted('GST_PACKAGE_ORIGIN', 'https://gstreamer.freedesktop.org')
configure_file(output : 'config.h', configuration : cdata)


# Pass -DHAVE_CONFIG_H to the conpiler so that C source
# knows to #include "config.h" for the macros defined above.
plugin_c_args = ['-DHAVE_CONFIG_H']

# GStreamer plugins are installed to <libdir>/gstreamer-1.0/
# so the runtime can discover them automatically via GST_PLUGIN_PATH.
plugins_install_dir = join_paths(get_option('libdir'), 'gstreamer-1.0')


# Build the plugin as a shared library.
# GStreamer loads plugins as shared libraries at runtime.
gstzstddec = library('gstzstddec',
  'src/gstzstddec.c',
  c_args: plugin_c_args,
  dependencies : [gst_dep, gstbase_dep, zstd_dep],
  install : true,
  install_dir : plugins_install_dir,
)