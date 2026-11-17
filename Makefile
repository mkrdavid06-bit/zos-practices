PRACTICES = practice_1_cat_grep practice_2_ls practice_3_chmod

.PHONY: all clean test $(PRACTICES)
all: $(PRACTICES)

$(PRACTICES):
	$(MAKE) -C $@

test: all
	python3 tests/test_utils.py

clean:
	@for practice in $(PRACTICES); do $(MAKE) -C $$practice clean || exit $$?; done
