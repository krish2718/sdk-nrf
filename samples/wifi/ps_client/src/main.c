
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
}