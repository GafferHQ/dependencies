TEMPLATE = subdirs

qtHaveModule(widgets) {
    SUBDIRS += widgets
    qtHaveModule(quick) {
      SUBDIRS += quick
  }
}

EXAMPLE_FILES += \
    common
