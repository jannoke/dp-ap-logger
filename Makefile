CXX = g++
PKG_CONFIG ?= pkg-config
MYSQLPP_CFLAGS := $(shell $(PKG_CONFIG) --cflags mysql++ 2>/dev/null)
MYSQLPP_LIBS := $(shell $(PKG_CONFIG) --libs mysql++ 2>/dev/null)
MYSQL_CFLAGS := $(shell mysql_config --cflags 2>/dev/null || mariadb_config --cflags 2>/dev/null || mariadb_config --include 2>/dev/null)
MYSQL_LIBS := $(shell mysql_config --libs 2>/dev/null || mariadb_config --libs 2>/dev/null)
ifeq ($(strip $(MYSQLPP_CFLAGS)),)
  CXXFLAGS = -Wall -O2 -std=c++11 -I src -I src/includes $(MYSQL_CFLAGS)
  LDFLAGS = -lmysqlpp $(MYSQL_LIBS) -lpthread
else
CXXFLAGS = -Wall -O2 -std=c++11 -I src -I src/includes $(MYSQLPP_CFLAGS)
LDFLAGS = $(MYSQLPP_LIBS) -lpthread
endif

SOURCES = src/dp-ap-logger.cpp src/includes/ConfigFile/ConfigFile.cpp
OBJECTS = $(SOURCES:.cpp=.o)
TARGET_DIR = build
TARGET = $(TARGET_DIR)/dp-ap-logger

all: $(TARGET)

$(TARGET): $(OBJECTS) | $(TARGET_DIR)
	$(CXX) $(OBJECTS) -o $(TARGET) $(LDFLAGS)

$(TARGET_DIR):
	mkdir -p $(TARGET_DIR)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET)
	rm -rf $(TARGET_DIR)

.PHONY: all clean
