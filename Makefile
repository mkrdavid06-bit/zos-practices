PRACTICES = lab_1 lab_2 lab_3

.PHONY: all clean test $(PRACTICES)
all: $(PRACTICES)

$(PRACTICES):
	$(MAKE) -C $@

test: all
	python3 tests/test_utils.py

clean:
	@for practice in $(PRACTICES); do $(MAKE) -C $$practice clean || exit $$?; done
