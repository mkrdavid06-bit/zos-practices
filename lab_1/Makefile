PRACTICES = practice_1_cat_grep practice_2_ls practice_3_chmod

.PHONY: all clean $(PRACTICES)
all: $(PRACTICES)

$(PRACTICES):
	$(MAKE) -C $@

clean:
	@for practice in $(PRACTICES); do $(MAKE) -C $$practice clean || exit $$?; done
