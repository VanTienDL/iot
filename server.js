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

// Arduino 1: DHT11 + TTP226
const port1 = new SerialPort({ path: 'COM3', baudRate: 9600 });
const parser1 = port1.pipe(new ReadlineParser({ delimiter: '\r\n' }));

// Arduino 2: Phan con lai
const port2 = new SerialPort({ path: 'COM4', baudRate: 9600 });
const parser2 = port2.pipe(new ReadlineParser({ delimiter: '\r\n' }));

app.use(express.static('public'));

let dht = { temp: 0, humi: 0 };
let soil = 0;
let lux = 0;
let fire = false;
let watering = false;
let light = false;
let buzzer = false;
let mode = false;

// === Xử lý Arduino 1 ===
parser1.on('data', (data) => {
  console.log('[COM3]', data);

  // DHT11
  if (data.startsWith('TEMP:')) {
    // TEMP:29C  | HUM:55%
    const tempMatch = data.match(/TEMP:(\d+)C/);
    const humiMatch = data.match(/HUM:(\d+)%/);

    if (tempMatch && humiMatch) {
      dht.temp = parseInt(tempMatch[1]);
      dht.humi = parseInt(humiMatch[1]);

      io.emit('dht', dht);
      masterSocket.emit('dht', dht); // gửi lên master server
    }
  } 
  //TTP26
  else if (data.startsWith('TTP:')) {
    const key = data.slice(4).trim();
    io.emit('key', key);
    masterSocket.emit('key', key);
  }
});



// === Xử lý Arduino 2 ===
parser2.on('data', (data) => {
  console.log('[COM4]', data);

  if (data.startsWith('DoAm:')) {
    soil = parseInt(data.slice(5).trim());
    io.emit('soil', soil);
    masterSocket.emit('soil', soil);
  } 
  else if (data.startsWith('AnhSang:')) {
    lux = parseInt(data.slice(8).trim());
    io.emit('lux', lux);
    masterSocket.emit('lux', lux);
  }
  else if (data.startsWith('Lua:')) {
    const value = data.slice(4).trim().toUpperCase();
    fire = (value === 'ON');
    io.emit('fire', fire);
    masterSocket.emit('fire', fire);
  }
  else if (data.startsWith('Bom:')) {
    const value = data.slice(4).trim().toUpperCase();
    watering = (value === 'ON');
    io.emit('watering', watering);
    masterSocket.emit('watering', watering);
  }
  else if (data.startsWith('Mode:')) {
    const value = data.slice(5).trim().toUpperCase();
    mode = (value === 'ON');
    io.emit('mode', mode);
    masterSocket.emit('mode', mode);
  }
  else if (data.startsWith('Den:')) {
    const value = data.slice(4).trim().toUpperCase();
    light = (value === 'ON' || value === 'BAT'); // hỗ trợ cả ON và BAT
    io.emit('light', light);
    masterSocket.emit('light', light);
  }
  else if (data.startsWith('Buzzer:')) {
    const value = data.slice(7).trim().toUpperCase();
    buzzer = (value === 'ON');
    io.emit('buzzer', buzzer);
    masterSocket.emit('buzzer', buzzer);
  }
});


// // Giao tiếp từ giao diện local
// io.on('connection', (socket) => {
//   socket.on('stop-buzzer', () => {
//     port1.write('STOP_BUZZER\n');
//     masterSocket.emit('stop-buzzer'); // gửi lệnh lên master nếu cần theo dõi/log
//   });

// });

// Lắng nghe sự kiện điều khiển từ master server

masterSocket.on('key', (key) => {
  console.log(`📥 Nhận lệnh KEY từ master: ${key}`);
  switch (key) {
    case '1':
      port2.write('BUZZER TAT\n');
      break;
    case '2':
      port2.write('BUZZER BAT\n');
      break;
    case '3':
      port2.write('DEN TAT\n');
      break;
    case '4':
      port2.write('DEN BAT\n');
      break;
    case '5':
      port2.write('BOM TAT\n');
      break;
    case '6':
      port2.write('BOM BAT\n');
      break;
    case '7':
      port2.write('MODE TAT\n');
      break;
    case '8':
      port2.write('MODE BAT\n');
      break;
    default:
      console.log('⚠️ Phím không hợp lệ:', key);
  }
});

masterSocket.on('stop-buzzer', () => {
  console.log('📥 Nhận lệnh từ master: STOP_BUZZER');
  port2.write('BUZZER TAT\n');
});
masterSocket.on('start-buzzer', () => {
  console.log('📥 Nhận lệnh từ master: START_BUZZER');
  port2.write('BUZZER BAT\n');
});
masterSocket.on('auto-buzzer', () => {
  console.log('📥 Nhận lệnh từ master: AUTO_BUZZER');
  port2.write('BUZZER AUTO\n');
});

masterSocket.on('stop-light', () => {
  console.log('📥 Nhận lệnh từ master: STOP_LIGHT');
  port2.write('DEN TAT\n');
});
masterSocket.on('start-light', () => {
  console.log('📥 Nhận lệnh từ master: START_LIGHT');
  port2.write('DEN BAT\n');
});
masterSocket.on('auto-light', () => {
  console.log('📥 Nhận lệnh từ master: AUTO_LIGHT');
  port2.write('DEN AUTO\n');
});

masterSocket.on('stop-pump', () => {
  console.log('📥 Nhận lệnh từ master: STOP_PUMP');
  port2.write('BOM TAT\n');
});
masterSocket.on('start-pump', () => {
  console.log('📥 Nhận lệnh từ master: START_PUMP');
  port2.write('BOM BAT\n');
});
masterSocket.on('auto-pump', () => {
  console.log('📥 Nhận lệnh từ master: AUTO_PUMP');
  port2.write('BOM AUTO\n');
});

masterSocket.on('stop-mode', () => {
  console.log('📥 Nhận lệnh từ master: STOP_MODE');
  port2.write('MODE TAT\n');
});
masterSocket.on('start-mode', () => {
  console.log('📥 Nhận lệnh từ master: START_MODE');
  port2.write('MODE BAT\n');
});

masterSocket.on('auto-all', () => {
  console.log('📥 Nhận lệnh từ master: AUTO_ALL');
  port2.write('AUTO ALL\n');
});


server.listen(3000, () => {
  console.log('🖥️  Local server chạy tại: http://localhost:3000');
});
