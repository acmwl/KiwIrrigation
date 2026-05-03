import { useState, useEffect } from 'react'; // Added useEffect here
import io from 'socket.io-client';
const socket = io(`http://${window.location.hostname}:3000`);

export default function App() {
  const [temperature, setTemperature] = useState('--');

  useEffect(() => {
    // Listen for the magic broadcast from the backend
    socket.on('sensorData', (data) => {
      console.log("Received from backend:", data);
      
      // If the topic is the status, update the temperature on the screen!
      if (data.topic === 'esp32/test/status') {
        setTemperature(data.message);
      }
    });
// Cleanup the connection if the component unmounts
    return () => socket.off('sensorData');
  }, []);

  return (
    <div style={{ padding: '20px' }}>
      <h1>My Smart Home</h1>
      <div style={{ border: '1px solid #ccc', padding: '20px' }}>
        <h2>Living Room Temp</h2>
        <h1 style={{ color: 'blue' }}>{temperature} °C</h1>
      </div>
    </div>
  );
}
