PRACTICES = lab_1 lab_2_fork lab_3_pipefifo

.PHONY: all clean test $(PRACTICES)
all: $(PRACTICES)

$(PRACTICES):
	$(MAKE) -C $@

test: all
	python3 tests/test_utils.py

clean:
	@for practice in $(PRACTICES); do $(MAKE) -C $$practice clean || exit $$?; done
