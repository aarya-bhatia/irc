#include "log.h"

int main()
{
	log_trace("Hello %s", "world");
	log_debug("Hello %s", "world");
	log_info("Hello %s", "world");
	log_warn("Hello %s", "world");
	log_error("Hello %s", "world");
	log_fatal("Hello %s", "world");

	return 0;
}
