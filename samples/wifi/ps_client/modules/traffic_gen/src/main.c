
traffic_gen_start(tg_config) {
    setup_ctrl_path();
    setup_data_path();
    start_traffic_gen();
    traffic_gen_wait_for_completion;
}

traffic_gen_wait_for_completion() {

}
traffic_gen_get_result() {

}