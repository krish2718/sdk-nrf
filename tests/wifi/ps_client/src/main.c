
const unsigned int test_payload_udp[10] = {1, 2, 4, 8, 16, 32, 64, 128, 256, 512};
const unsigned int test_expected_payload_udp[10] = {1, 2, 4, 8, 16, 32, 64, 128, 256, 512};

void init()
{
    //init wifi
    //init udp
    //init tcp
}

void setup()
{
    //connect to wifi
    //connect to udp
    //connect to tcp
}

ZTEST(wifi_udp_tests, payload_tests)
{

    init();
    setup();

    for (int i = 0; i < 10; i++)
    {
        tput = start_traffic(test_payload_udp[i]);
        ASSERT(tput >= test_expected_payload_udp[i]);
    }
}


ZTEST(wifi_tcp_tests, payload_tests)
{

    init();
    setup();
    tput = start_traffic();

    ASSERT(tpuy >= 1024 * 1024);
}