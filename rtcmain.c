/* vim:set tw=80 ts=8 sw=8 sts=8 cc=80 noet cino+=t0: */

#include "rtclib.h"

#include <assert.h>
#include <time.h>

struct rtc *rtc_parse(struct rtc *rtc, const char *file) {
	FILE *f;
	char c;
	int state = 0, lit = 0, sign = 0;
	if(!(f = fopen(file, "r"))) {
		rtc->status = 4;
		return rtc;
	}
	int v_size = 0, c_size = 0;
	while((c = fgetc(f)) != EOF) {
		switch(state) {
		case 0:
			state = c == 'p' ? 1 : 6;
			break;
		case 1:
			assert(c == ' ');
			state = 2;
			break;
		case 2:
			assert(c == 'c');
			state = 3;
			break;
		case 3:
			assert(c == 'n');
			state = 4;
			break;
		case 4:
			assert(c == 'f');
			state = 5;
			break;
		case 5:
			assert(c == ' ');
			state = 7;
			break;
		case 6:
			if(c == '\n')
				state = 0;
			break;
		case 7:
			if(isdigit(c))
				v_size = v_size * 10 + c - '0';
			else
				state = 8;
			break;
		case 8:
			if(isdigit(c))
				c_size = c_size * 10 + c - '0';
			else {
				state = 9;
				rtc->v_size = v_size + 1;
				rtc->c_size = c_size + 1;
				rtc = rtc_new(rtc);
				rtc->c_count = 1;
			}
			break;
		case 9:
			sign = c == '-' ? -1 : 1;
			lit = isdigit(c) ? (c - '0') : 0;
			state = 10;
			break;
		case 10:
			if(isdigit(c))
				lit = 10 * lit + (c - '0');
			else
				rtc_add(rtc, sign * lit), state = 9;
			break;
		};
	}
	fclose(f);
	return rtc;
}

struct rtc *rtc_main(struct rtc *rtc, int argc, char *argv[]) {
	rtc->out_log = stderr;
	if(argc < 2)
		rtc->status = 1;
	if(argc > 2 && !(rtc->out_formula = fopen(argv[2], "w")))
		rtc->status = 2;
	if(argc > 3 && !(rtc->out_proof = fopen(argv[3], "w")))
		rtc->status = 3;
	rtc = rtc_parse(rtc, argv[1]);
	if(rtc->status)
		return rtc;
	rtc_log(rtc, "input cnf:     %8d variables, %12d clauses",
		rtc->v_size - 1, rtc->c_count - 1);
	rtc_init(rtc);
	rtc_preprocess(rtc);
	rtc_log(rtc, "output cnf:    %8d variables, %12d clauses",
		rtc->v_size - 1, rtc->c_count - 1);
	if(rtc->out_formula)
		rtc_print(rtc->out_formula, rtc);
	/* rtc_stats(rtc); */
	return rtc;
}

int main(int argc, char *argv[]) {
	int status;
	struct rtc *rtc;
	time_t start_time, end_time;
	time(&start_time);
	rtc = calloc(1, sizeof(rtc[0]));
	rtc = rtc_main(rtc, argc, argv);
	time(&end_time);
	if((status = rtc->status))
		rtc_log(rtc, "usage: %s <input.cnf> <preprocessed> <proof>",
			argv[0]);
	else
		rtc_log(rtc, "elapsed time: %ds",
			(int)difftime(end_time, start_time));
	rtc_abandon(rtc);
	return status;
}
