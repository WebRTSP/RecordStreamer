[![webrtsp-record-streamer](https://snapcraft.io/webrtsp-record-streamer/badge.svg)](https://snapcraft.io/webrtsp-record-streamer)

## Record Streamer
Media URLs Recorder to WebRTSP server

### How to install it as Snap package
1. Run: `sudo snap install webrtsp-record-streamer --edge`;
2. To see application logs in realtime you can run `sudo snap logs webrtsp-record-streamer -f`;

### How to edit config file
1. `sudoedit /var/snap/webrtsp-record-streamer/common/record-streamer.conf`;
2. To load updated config it's required to restart Snap: `sudo snap restart webrtsp-record-streamer`;

### How to use it as streamer for Cloud DVR
1. Install [rtsp-to-webrtsp](https://github.com/WebRTSP/ReStreamer#how-to-install-it-as-snap-package-and-try) on some VPS/VDS/Dedicated;
2. Configure `rtsp-to-webrtsp` as described [here](https://github.com/WebRTSP/ReStreamer#how-to-use-it-as-cloud-dvr-for-ip-cam-not-accessable-directly);
3. Install `webrtsp-record-streamer` Snap package ([as described above](#how-to-install-it-as-snap-package)) on some device (you can use something like Raspberry Pi) on network where IP Cam is accessible directly;
4. Open config file for edit ([as described above](#how-to-edit-config-file)) and replace content with something like
```
target: {
  host: "example.com" // IP or DNS of host were rtsp-to-webrtsp is installed
  port: 5554 // or 5555 if TLS is enabled (highly recommended)
  uri: "DVR" // use the same name as was used in rtsp-to-webrtsp's config
  token: "some-random-string" // use the same string as used in rtsp-to-webrtsp's config
}

source: {
  url: "rtsp://your_cam_ip_or_dns:port/path"
}

stun-server: "stun://stun.l.google.com:19302"
```
5. Restart Snap: `sudo snap restart webrtsp-record-streamer`;


### How to use it as streamer for Cloud DVR with motion detection
1. Install [rtsp-to-webrtsp](https://github.com/WebRTSP/ReStreamer#how-to-install-it-as-snap-package-and-try) on some VPS/VDS/Dedicated;
2. Configure `rtsp-to-webrtsp` as described [here](https://github.com/WebRTSP/ReStreamer#how-to-use-it-as-cloud-dvr-for-ip-cam-not-accessible-directly);
3. Install `webrtsp-record-streamer` Snap package ([as described above](#how-to-install-it-as-snap-package)) on some device (you can use something like Raspberry Pi) on network where IP Cam is accessible directly;
4. Open config file for edit ([as described above](#how-to-edit-config-file)) and replace content with something like
```
target: {
  host: "example.com" // IP or DNS of host were rtsp-to-webrtsp is installed
  port: 5554 // or 5555 if TLS is enabled (highly recommended)
  uri: "DVR" // use the same name as was used in rtsp-to-webrtsp's config
  token: "some-random-string" // use the same string as used in rtsp-to-webrtsp's config
}

source: {
  onvif: "http://dns_or_ip_for_cam_with_onvif_support:port/"
  track-motion-event: true // true if it's required to stream only if motion is detected by IP Cam
  motion-record-time: 10 // specify how many seconds it's required to stream after last motion detected by IP Cam
}

stun-server: "stun://stun.l.google.com:19302"
```
5. Restart Snap: `sudo snap restart webrtsp-record-streamer`;
