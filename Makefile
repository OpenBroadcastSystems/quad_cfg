SDK_PATH=/home/user/Blackmagic\ DeckLink\ SDK\ 10.5.5/Linux/include
CFLAGS=-I $(SDK_PATH)
LDFLAGS=-ldl -lpthread

all:
	c++ -o quad_cfg quad_cfg.cpp $(SDK_PATH)/DeckLinkAPIDispatch.cpp $(CFLAGS) $(LDFLAGS)

clean:
	rm -f quad_cfg
