const express = require('express');
const { SerialPort } = require('serialport');
const { ReadlineParser } = require('@serialport/parser-readline');
const http = require('http');
const socketIo = require('socket.io');
const { io: ioClient } = require('socket.io-client'); // <-- import client socket.io

const app = express();
const server = http.createServer(app);
const io = socketIo(server);

// Kết nối tới master server trên Koyeb
const masterSocket = ioClient('https://isolated-nerte-phuongdinh-ce9dcdfb.koyeb.app');
masterSocket.on('connect', () => {
  console.log('🔗 Kết nối tới master server thành công');
  masterSocket.emit('register-local'); // <-- báo hiệu mình là server local
});

// Arduino 1: DHT11 + KY026
const port1 = new SerialPort({ path: 'COM3', baudRate: 9600 });
const parser1 = port1.pipe(new ReadlineParser({ delimiter: '\r\n' }));

// // Arduino 2: Soil + Relay + TTP226
// const port2 = new SerialPort({ path: 'COM4', baudRate: 9600 });
// const parser2 = port2.pipe(new ReadlineParser({ delimiter: '\r\n' }));

app.use(express.static('public'));

let dht = { temp: 0, humi: 0 };
let soil = 0;
let fire = false;
let watering = false;

// === Xử lý Arduino 1 ===
parser1.on('data', (data) => {
  console.log('[COM3]', data);

  if (data.startsWith('DHT:')) {
    const parts = data.slice(4).split(',');
    dht.temp = parseInt(parts[0]);
    dht.humi = parseInt(parts[1]);

    io.emit('dht', dht);
    masterSocket.emit('dht', dht); // gửi lên master server
  } 
  else if (data.startsWith('FIRE:')) {
    fire = data.slice(5) === '1';

    io.emit('fire', fire);
    masterSocket.emit('fire', fire);
  } 
  else if (data.startsWith('TTP:')) {
    const key = data.slice(4).trim();
    io.emit('key', key);
    masterSocket.emit('key', key);
  }
});

// === Xử lý Arduino 2 ===
// parser2.on('data', (data) => {
//   console.log('[COM4]', data);

//   if (data.startsWith('SOIL:')) {
//     soil = parseInt(data.slice(5));

//     io.emit('soil', soil);
//     masterSocket.emit('soil', soil);
//   } 
//   else if (data.startsWith('TTP:')) {
//     const key = data.slice(4).trim();
//     io.emit('key', key);
//     masterSocket.emit('key', key);
//   }
// });

// Giao tiếp từ giao diện local
io.on('connection', (socket) => {
  socket.on('stop-buzzer', () => {
    port1.write('STOP_BUZZER\n');
    masterSocket.emit('stop-buzzer'); // gửi lệnh lên master nếu cần theo dõi/log
  });

  socket.on('toggle-water', () => {
    watering = !watering;
    const cmd = watering ? 'WATER_ON\n' : 'WATER_OFF\n';
    port2.write(cmd);

    io.emit('watering', watering);
    masterSocket.emit('watering', watering);
  });
});

// Lắng nghe sự kiện điều khiển từ master server
masterSocket.on('stop-buzzer', () => {
  console.log('📥 Nhận lệnh từ master: STOP_BUZZER');
  port1.write('STOP_BUZZER\n');
});

masterSocket.on('toggle-water', () => {
  console.log('📥 Nhận lệnh từ master: TOGGLE_WATER');
  watering = !watering;
  const cmd = watering ? 'WATER_ON\n' : 'WATER_OFF\n';
  port2.write(cmd);
  io.emit('watering', watering); // cập nhật lại trạng thái tưới trên local UI
});

server.listen(3000, () => {
  console.log('🖥️  Local server chạy tại: http://localhost:3000');
});
