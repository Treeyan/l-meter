
CC=sdcc
target=lmeter

all: 
	$(CC) --iram-size 512 --opt-code-size --no-xinit-opt --model-large $(target).c
	packihx < $(target).ihx > $(target).hex
	
.PHONY: clean

clean:
	rm -f $(target).a* $(target).i* $(target).l* $(target).m* $(target).r* $(target).s*

