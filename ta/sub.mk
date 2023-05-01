global-incdirs-y += include
srcs-y += cnn_ta.c \
	sod.c \
	aes.c
libdirs += lib/
libnames += m
libdeps += lib/libm.a