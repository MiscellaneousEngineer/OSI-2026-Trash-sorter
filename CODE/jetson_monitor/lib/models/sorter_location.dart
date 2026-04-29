/// A demo deployment of an S.T.R.I.A sorting machine.
///
/// All values are hardcoded for the demo — these aren't real installations.
class SorterLocation {
  final String name;
  final String address;
  final double lat;
  final double lng;
  final String status; // 'Online', 'Offline', 'Maintenance'

  const SorterLocation({
    required this.name,
    required this.address,
    required this.lat,
    required this.lng,
    required this.status,
  });
}
