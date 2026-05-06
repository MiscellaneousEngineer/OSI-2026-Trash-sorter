/// App-wide configuration.
///
/// Change [jetsonIp] to the address of the Jetson on your network.
class AppConfig {
  static const String jetsonIp = '192.168.1.100';
  static const int jetsonPort = 5000;  // Flask dashboard port

  static String get httpBase => 'http://$jetsonIp:$jetsonPort';
}
