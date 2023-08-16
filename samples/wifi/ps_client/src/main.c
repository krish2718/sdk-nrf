int setup_twt()
{

    send_twt_setup();
    int ret = wait_for_twt_response();
    if (ret == 0)
    {
        printf("twt setup success\n");
    }
    else
    {
        printf("twt setup failed\n");
    }
}

int wifi_connect()
{
    return 0;
}

int main()
{

    int ret = wifi_connect();
    if (ret == 0)
    {
        printf("wifi connect success\n");
    }
    else
    {
        printf("wifi connect failed\n");
    }

    setup_twt();

    struct traffic_gen_config tg_config = {
        .type = CONFIG_WIFI_TWT_UDP,
        .role = CONFIG_WIFI_TWT_CLIENT,
        .dut_ip = CONFIG_DUT_IP,
    }

    traffic_gen_start(tg_config);
    traffic_gen_wait_for_completion();
    traffic_gen_get_result();
}