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
}