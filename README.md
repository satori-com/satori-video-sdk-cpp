# librtmvideo
## To use publisher cli

To use video file as a source:
```shell
./bin/rtmvideo_publisher_cli --endpoint <rtm-endpoint> --port <rtm-port> --appkey <rtm-appkey> --channel <rtm-channel> --input-video-file test.mp4
```

To use replay file as a source:
```shell
./bin/rtmvideo_publisher_cli --endpoint <rtm-endpoint> --port <rtm-port> --appkey <rtm-appkey> --channel <rtm-channel> --input-replay-file test.replay
```

To use camera as a source:
```shell
./bin/rtmvideo_publisher_cli --endpoint <rtm-endpoint> --port <rtm-port> --appkey <rtm-appkey> --channel <rtm-channel> --input-camera
```

## To use recorder cli

To record video from RTM stream
```shell
./bin/rtmvideo_recorder_cli --endpoint <rtm-endpoint> --port <rtm-port> --appkey <rtm-appkey> --channel <rtm-channel> --output-video-file ~/test.mkv
```

Or to specify index space (for improved file seeking)
```shell
./bin/rtmvideo_recorder_cli --endpoint <rtm-endpoint> --port <rtm-port> --appkey <rtm-appkey> --channel <rtm-channel> --output-video-file ~/test.mkv --reserved-index-space 50000
```

In case reserved index space is not enough, one can fix index section by using `FFmpeg`:
```shell
ffmpeg -i ~/test.mkv -c copy -reserve_index_space 100000 ~/test_fixed.mkv
```