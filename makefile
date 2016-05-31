TARGET=ratemaster
SRC=$(RATEUTIL) 

RATEUTIL=-c ratemaster.cc -c rate-limiter.cpp -c configuration.cc

all: $(TARGET)
install: $(TARGET)_install
clean:
	rm *.lo *~ $(TARGET).so
$(TARGET):
	tsxs -o $@.so $(SRC)
$(TARGET)_install:
	tsxs -o $(TARGET).so -i