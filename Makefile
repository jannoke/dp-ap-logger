CXX = g++
CXXFLAGS = -Wall -O2 -std=c++11 -I src -I src/includes
LDFLAGS = -lmysqlclient -lpthread

SOURCES = src/dp-ap-logger.cpp src/includes/ConfigFile/ConfigFile.cpp
OBJECTS = $(SOURCES:.cpp=.o)
TARGET = dp-ap-logger

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(OBJECTS) -o $(TARGET) $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET)

.PHONY: all clean
