PROGRAMS := transform trim joytrack record playback server status idl matrix-test
OBJS := config i2c pca9685 stewart matrix delay

SRCDIR := src
OBJDIR := out
BINDIR := bin
CFLAGS = -Wall -Werror -DVERSION=\"0.1\"

OBJECTS  := $(SOURCES:$(SRCDIR)/%.c=$(OBJDIR)/%.o)

all: $(OBJDIR) $(BINDIR) $(addprefix $(BINDIR)/,$(PROGRAMS))

$(OBJDIR)/%.o : $(SRCDIR)/%.c $(SRCDIR)/config.h
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR):
	mkdir $(OBJDIR)

$(BINDIR):
	mkdir $(BINDIR)

$(OBJDIR)/%.o:$(INDIR)%.c config.h
	gcc $(CFLAGS) -c -g -O -o $@ $<

$(BINDIR)/status: $(OBJDIR)/status.o $(addprefix $(OBJDIR)/,$(addsuffix .o,$(OBJS)))
	gcc $(CFLAGS) -g -o $@ $^ -lm

$(BINDIR)/server: $(OBJDIR)/server.o $(addprefix $(OBJDIR)/,$(addsuffix .o,$(OBJS)))
	gcc $(CFLAGS) -g -o $@ $^ -lm

$(BINDIR)/joytrack: $(OBJDIR)/joytrack.o $(addprefix $(OBJDIR)/,$(addsuffix .o,$(OBJS)))
	gcc $(CFLAGS) -g -o $@ $^ -lm

$(BINDIR)/trim: $(OBJDIR)/trim.o $(addprefix $(OBJDIR)/,$(addsuffix .o,$(OBJS)))
	gcc $(CFLAGS) -g -o $@ $^ -lm

$(BINDIR)/transform: $(OBJDIR)/transform.o $(addprefix $(OBJDIR)/,$(addsuffix .o,$(OBJS)))
	gcc $(CFLAGS) -g -o $@ $^ -lm

$(BINDIR)/matrix-test: $(OBJDIR)/matrix-test.o $(addprefix $(OBJDIR)/,$(addsuffix .o,$(OBJS)))
	gcc $(CFLAGS) -g -o $@ $^ -lm

$(BINDIR)/record: $(OBJDIR)/record.o $(addprefix $(OBJDIR)/,$(addsuffix .o,$(OBJS)))
	gcc $(CFLAGS) -g -o $@ $^ -lm

$(BINDIR)/playback: $(OBJDIR)/playback.o $(addprefix $(OBJDIR)/,$(addsuffix .o,$(OBJS)))
	gcc $(CFLAGS) -g -o $@ $^ -lm

$(BINDIR)/idl: $(OBJDIR)/idl.o $(addprefix $(OBJDIR)/,$(addsuffix .o,$(OBJS)))
	gcc $(CFLAGS) -g -o $@ $^ -lm


clean:
	rm -rf $(BINDIR) $(OBJDIR)
