# vi:set ft=perl ts=4 sw=4 et fdm=marker:

use Test::Nginx::Socket;

plan tests => repeat_each(2) * blocks() * 2;
no_shuffle();
run_tests();

__DATA__

=== TEST 1: /status/format/html
--- main_config
    stream {
        server_traffic_status_zone;
    }
--- http_config
    stream_server_traffic_status_zone;
--- config
    location /status {
        stream_server_traffic_status_display;
        stream_server_traffic_status_display_format html;
    }
--- request
GET /status/format/html
--- response_headers_like
Content-Type: text/html



=== TEST 2: /status
--- main_config
    stream {
        server_traffic_status_zone;
    }
--- http_config
    stream_server_traffic_status_zone;
--- config
    location /status {
        stream_server_traffic_status_display;
        stream_server_traffic_status_display_format html;
    }
--- request
GET /status
--- response_headers_like
Content-Type: text/html
