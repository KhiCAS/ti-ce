APP_TOOLS_DIR := $(realpath $(dir $(lastword $(MAKEFILE_LIST))))

INIT_LOC = 0
LINKER_SCRIPT = $(APP_TOOLS_DIR)/linker_script
OUTPUT_MAP = NO

APP_SRC_FILE = $(APP_TOOLS_DIR)/app.src

DEPS := $(APP_SRC_FILE) $(APP_TOOLS_DIR)/makefile $(DEPS)

EXTRA_LDFLAGS += \
	-i $(call QUOTE_ARG,provide __app_name = "$(APP_NAME)") \
	-i $(call QUOTE_ARG,provide __app_version = "$(APP_VERSION)") \
	-i $(call QUOTE_ARG,provide __app_desc = "$(DESCRIPTION)")

default: app # install_prog

include $(shell cedev-config --makefile)

TARGET8EK ?= $(NAME).8ek
TARGETBIN ?= $(NAME).bin
APP_INST_NAME ?= APPINST
APP_INST_DESC ?= App Installer
APP_INST_VAR_PREFIX ?= AppIns

app: $(BINDIR)/$(TARGET8EK)
# install_prog: $(BINDIR)/AppIns0.8xv $(BINDIR)/INSTALL.8xp

$(BINDIR)/$(TARGETBIN): $(BINDIR)/$(TARGETOBJ) $(MAKEFILE_LIST) $(DEPS)
	$(Q)$(call MKDIR,$(@D))
	$(Q)(OBJCOPY) -O binary $(call QUOTE_ARG,$<) $(call QUOTE_ARG,$@)

$(BINDIR)/$(TARGET8EK): $(BINDIR)/$(TARGETBIN) $(APP_TOOLS_DIR)/make_8ek.py
	python3 $(APP_TOOLS_DIR)/make_8ek.py $(BINDIR)/$(TARGETBIN) $(BINDIR)/$(TARGET8EK) $(NAME)

$(BINDIR)/AppIns0.8xv: $(BINDIR)/$(TARGETBIN) $(APP_TOOLS_DIR)/make_segments.py
	python3 $(APP_TOOLS_DIR)/make_segments.py $(BINDIR)/$(TARGETBIN) $(BINDIR)

$(BINDIR)/INSTALL.8xp:
	BINDIR=$(realpath $(BINDIR)) \
	OBJDIR=$(realpath $(OBJDIR))/inst \
	APP_BIN=$(realpath $(BINDIR)/$(TARGETBIN)) \
	NAME=$(APP_INST_NAME) \
	DESCRIPTION="$(APP_INST_DESC)" \
	VAR_PREFIX="$(APP_INST_VAR_PREFIX)" \
	$(MAKE) -C $(APP_TOOLS_DIR)/installer

.PHONY: default app # install_prog
