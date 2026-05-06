import 'package:flutter/material.dart';
import 'package:flutter_map/flutter_map.dart';
import 'package:latlong2/latlong.dart';
import '../models/sorter_location.dart';

class MapScreen extends StatelessWidget {
  const MapScreen({super.key});

  // Hardcoded demo deployments around the Toulouse metropolitan area.
  // Edit this list to add / move / rename machines for the demo.
  static const List<SorterLocation> _locations = [
    SorterLocation(
      name: 'Capitole',
      address: 'Place du Capitole, Toulouse',
      lat: 43.6045,
      lng: 1.4442,
      status: 'Online',
    ),
    SorterLocation(
      name: 'Colomiers',
      address: 'Centre-ville, Colomiers',
      lat: 43.6128,
      lng: 1.3372,
      status: 'Online',
    ),
    SorterLocation(
      name: 'Blagnac',
      address: 'Aéroport Toulouse-Blagnac',
      lat: 43.6358,
      lng: 1.3892,
      status: 'Online',
    ),
    SorterLocation(
      name: 'Tournefeuille',
      address: 'Place de la Mairie, Tournefeuille',
      lat: 43.5803,
      lng: 1.3475,
      status: 'Maintenance',
    ),
    SorterLocation(
      name: 'Labège',
      address: 'Innopole, Labège',
      lat: 43.5447,
      lng: 1.5184,
      status: 'Online',
    ),
  ];

  Color _statusColor(String status) {
    switch (status) {
      case 'Online':      return Colors.greenAccent;
      case 'Maintenance': return Colors.orangeAccent;
      case 'Offline':     return Colors.redAccent;
      default:            return Colors.white54;
    }
  }

  void _showLocationSheet(BuildContext context, SorterLocation loc) {
    showModalBottomSheet(
      context: context,
      backgroundColor: const Color(0xFF1A1A1A),
      shape: const RoundedRectangleBorder(
        borderRadius:
            BorderRadius.vertical(top: Radius.circular(16)),
      ),
      builder: (_) => Padding(
        padding: const EdgeInsets.fromLTRB(20, 16, 20, 28),
        child: Column(
          mainAxisSize: MainAxisSize.min,
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Center(
              child: Container(
                width: 40,
                height: 4,
                decoration: BoxDecoration(
                  color: Colors.white24,
                  borderRadius: BorderRadius.circular(2),
                ),
              ),
            ),
            const SizedBox(height: 16),
            Row(
              children: [
                Icon(Icons.location_on,
                    color: _statusColor(loc.status), size: 28),
                const SizedBox(width: 8),
                Expanded(
                  child: Text(
                    'S.T.R.I.A — ${loc.name}',
                    style: const TextStyle(
                      color: Colors.white,
                      fontSize: 18,
                      fontWeight: FontWeight.bold,
                    ),
                  ),
                ),
              ],
            ),
            const SizedBox(height: 12),
            Text(loc.address,
                style: const TextStyle(color: Colors.white70)),
            const SizedBox(height: 8),
            Row(
              children: [
                const Text('Status: ',
                    style: TextStyle(color: Colors.white54)),
                Text(loc.status,
                    style: TextStyle(
                      color: _statusColor(loc.status),
                      fontWeight: FontWeight.bold,
                    )),
              ],
            ),
            const SizedBox(height: 8),
            Text(
              '${loc.lat.toStringAsFixed(4)}, ${loc.lng.toStringAsFixed(4)}',
              style: const TextStyle(color: Colors.white38, fontSize: 12),
            ),
          ],
        ),
      ),
    );
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      backgroundColor: const Color(0xFF0D0D0D),
      appBar: AppBar(
        title: const Text('Find a Sorter'),
        backgroundColor: const Color(0xFF0D0D0D),
        foregroundColor: Colors.greenAccent,
      ),
      body: FlutterMap(
        options: const MapOptions(
          initialCenter: LatLng(43.6045, 1.4442),
          initialZoom: 11,
          minZoom: 3,
          maxZoom: 18,
        ),
        children: [
          TileLayer(
            urlTemplate:
                'https://tile.openstreetmap.org/{z}/{x}/{y}.png',
            userAgentPackageName: 'com.osi2026.jetson_monitor',
          ),
          MarkerLayer(
            markers: _locations
                .map((loc) => Marker(
                      point: LatLng(loc.lat, loc.lng),
                      width: 50,
                      height: 50,
                      alignment: Alignment.topCenter,
                      child: GestureDetector(
                        onTap: () => _showLocationSheet(context, loc),
                        child: Icon(
                          Icons.location_on,
                          color: _statusColor(loc.status),
                          size: 40,
                          shadows: const [
                            Shadow(
                                blurRadius: 6,
                                color: Colors.black87,
                                offset: Offset(0, 2)),
                          ],
                        ),
                      ),
                    ))
                .toList(),
          ),
        ],
      ),
    );
  }
}
