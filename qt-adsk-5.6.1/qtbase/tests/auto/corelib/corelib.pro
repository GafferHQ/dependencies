TEMPLATE=subdirs

SUBDIRS = \
   kernel

!ios: SUBDIRS += \
   animation \
   codecs \
   global \
   io \
   itemmodels \
   json \
   mimetypes \
   plugin \
   statemachine \
   thread \
   tools \
   xml
