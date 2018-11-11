const SerialPort = require('serialport');
const crc32 = require('crc-32');
const { promisify } = require('util');
const fs = require('fs');
const readFilePromise = promisify(fs.readFile);

function add_crc32(buf) {
  var crc = crc32.buf(buf);
  return Buffer.concat([
    buf,
    new Buffer([
      crc         & 0xFF,
      (crc >> 8)  & 0xFF,
      (crc >> 16) & 0xFF,
      (crc >> 24) & 0xFF
    ])
  ]);
}

function get_random_bytes(num) {
  var b = [];
  for (let i = 0; i < num; i++) {
    b.push(Math.floor(Math.random() * 0x100));
  }
  return b;
}

function parse_packet(buffer) {
  if (buffer.length < 12) return null; // this must not be a packet.
  let len = buffer[7];
  if (buffer.length != (12 + len)) return null; // this must not be a packet.
  return {
    id: buffer[5],
    op: buffer[6],
    payload: [...(buffer.slice(8,12+len-4))],
  };
}

function send(port, data, timeout_ms) {
  return new Promise((resolve, reject) => {
    var timeout_timer = setTimeout(() => {
      reject('send timeout: ' + data);
    }, timeout_ms);
    port.flush(function(e) {
      if (e) reject(e);
      // console.log('Send:', send_times++, data);
      port.write(data, function(e) {
        if (e) reject(e);
        port.drain(function (e) {
          if (e) reject(e);
          clearTimeout(timeout_timer);
          resolve();
        });
      });
    });
  });
}

var receivingPromises = {};

function recv(id, timeout_ms) {
  return new Promise((resolve, reject) => {
    var timeout_timer = setTimeout(() => {
      reject('recv timeout: ' + id);
      receivingPromises[id] = null;
    }, timeout_ms);
    receivingPromises[id] = [ resolve, timeout_timer ];
  });
}

function read16(buffer, pos) {
  var rt = 0;
  rt |= buffer[pos];
  rt |= buffer[pos+1] << 8;
  return rt;
}

async function command(port, packet, send_timeout_ms=1000, recv_timeout_ms=1000) {
  var id = packet[5]
  await send(port, packet, send_timeout_ms);
  return parse_packet(await recv(id, recv_timeout_ms));
}

function serializable_tbl(tbl) {
  let rev_tbl = {};
  Object.keys(tbl).forEach(k => {
    rev_tbl[tbl[k]] = k;
  });
  return {
    serialize: k => tbl[k],
    deserialize: k => rev_tbl[k]
  };
}

var RT_VAL_TBL = serializable_tbl({
  "ACK":  0x79,
  "NACK": 0x1F,
});

var CHIP_TYPE_TBL = serializable_tbl({
  "STM32F103RB": 0x410,
  "STM32L152RG": 0x415,
  "STM32L053R8": 0x417,
  "STM32F446RE": 0x421,
  "STM32F411RE": 0x431,
  "STM32F401RE": 0x433,
  "STM32L152RE": 0x437,
  "STM32F302R8": 0x439,
  "STM32F334R8": 0x438,
  "STM32F030R8": 0x440,
  "STM32F091RC": 0x442,
  "STM32F303RE": 0x446,
  "STM32L073RZ": 0x447,
  "STM32F070RB/STM32F072RB": 0x448,
  "STM32F410RB": 0x458,
});

var RUN_MODE_TBL = serializable_tbl({
  "APP_MODE": 0x00,
  "FLASHER_MODE": 0x01,
});

var FIRM_PARTITION_TBL = serializable_tbl({
  "FIRM0": 0x00,
  "FIRM1": 0x01,
});

function get_next_firm(curr_firm) {
  return (curr_firm === 'FIRM0' ? 'FIRM1' : 'FIRM0');
}

var get_packet_body = (() => {
  var id = 0;
  return function get_packet_body(op, payload) {
    return new Buffer([
      0xAA, 0x55,     // preamble
      0x00,           // type=command
      0x00,           // sender 0
      0x01,           // dest 1
      (id++) & 0xFF,  // packet_id 0
      op,             // operator
      payload.length, // len
    ].concat(payload));
  };
})();

var OP_GET_STATUS  = 0x01;
var OP_ERASE       = 0x02;
var OP_WRITE_START = 0x03;
var OP_WRITE_DATA  = 0x04;
var OP_WRITE_END   = 0x05;
var OP_RESET       = 0x06;

function create_packet(name, ...args) {
  switch (name) {
    case "GET_STATUS":
      return add_crc32(get_packet_body(OP_GET_STATUS, []));
    case "ERASE":
      return add_crc32(get_packet_body(OP_ERASE, [FIRM_PARTITION_TBL.serialize(args[0])]));
    case "WRITE_START":
      return add_crc32(get_packet_body(OP_WRITE_START, [FIRM_PARTITION_TBL.serialize(args[0])]));
    case "WRITE_DATA":
      return add_crc32(get_packet_body(OP_WRITE_DATA, args[0]));
    case "WRITE_END":
      return add_crc32(get_packet_body(OP_WRITE_END, []));
    case "RESET":
      return add_crc32(get_packet_body(OP_RESET, [
        RUN_MODE_TBL.serialize(args[0]),
        FIRM_PARTITION_TBL.serialize(args[1]),
      ]));
  }
}

async function sequence(port) {
  let curr_firm;
  {
    console.log("## GET_STATUS ==========================");
    let status = await command(port, create_packet('GET_STATUS'));
    if (!status || RT_VAL_TBL.deserialize(status.payload[0]) !== "ACK")
      throw Error('Get status failed.');
    let chip_type = CHIP_TYPE_TBL.deserialize(read16(status.payload, 1));
    let run_mode  = RUN_MODE_TBL.deserialize(status.payload[3]);
    curr_firm = FIRM_PARTITION_TBL.deserialize(status.payload[4]);
    console.log('chip_type:', chip_type);
    console.log('run_mode:', run_mode);
    console.log('curr_firm:', curr_firm);
  }
  let target_firm = get_next_firm(curr_firm);
  console.log('target_firm:', target_firm);
  {
    console.log("## ERASE_FIRM ==========================");
    process.stdout.write('erasing: '+target_firm);
    let int_id = setInterval(() => { process.stdout.write("."); }, 500);
    let result = await command(port, create_packet('ERASE', target_firm), 1000, 1000 * 60);
    clearInterval(int_id);
    if (!result || RT_VAL_TBL.deserialize(result.payload[0]) !== "ACK")
      throw new Error("Erase firm failed.");
    console.log('Done.');
  }
  {
    console.log("## WRITE_FIRM ==========================");
    let result;
    console.log('Start writing...');
    result = await command(port, create_packet('WRITE_START', target_firm));
    if (!result || RT_VAL_TBL.deserialize(result.payload[0]) !== "ACK")
      throw new Error("Start writing failed.");
    let firmware = await readFilePromise(`../example-app/build/app-firmware.${target_firm}.bin`);
    let firmware_len = firmware.length;
    console.log(`Read firm ... ${firmware_len} bytes.`);
    let i = 0;
    while (i < firmware_len) {
      let result = await command(port, create_packet('WRITE_DATA', [...firmware.slice(i, i+192)]));
      if (!result || RT_VAL_TBL.deserialize(result.payload[0]) !== "ACK")
        throw new Error(`Writing failed. on ${i} ... ${i+192}.`);
      i += 192;
      let written_bytes = Math.min(i, firmware_len);
      console.log(`Written firm ... ${written_bytes} bytes (${Math.round((written_bytes / firmware_len)*100)}%)`);
    }
    console.log(`End writing...`);
    result = await command(port, create_packet('WRITE_END'));
    if (!result || RT_VAL_TBL.deserialize(result.payload[0]) !== "ACK")
      throw new Error(`End writing failed.`);
    console.log('Done.');
  }
  {
    console.log("## RESET ==========================");
    let result = await command(port, create_packet('RESET', 'APP_MODE', target_firm));
    if (!result || RT_VAL_TBL.deserialize(result.payload[0]) !== "ACK")
      throw new Error(`Reset failed.`);
    console.log('Done.');
  }
}

SerialPort.list(function(err, ports) {
  if (err) { console.error(err); return; }
  var p = ports.find(p => p.manufacturer === 'smihica');
  if (!p) {
    console.error("Open port failed.");
    process.exit(1);
  }
  var comName = p.comName;
  var port = new SerialPort(comName, { baudRate: 115200 * 8, autoOpen: false });
  var sent = new Buffer(0);
  var checked_count = 0;
  var sent_count = 0;
  var checking_count_max = 50000;
  port.on('data', function(d) {
    var id = d[5];
    var cb = receivingPromises[id];
    if (cb) {
      let [resolve, timeout_timer] = cb;
      clearTimeout(timeout_timer);
      resolve(d);
    }
  });
  port.open(function (err) {
    if (err) return console.error('Error opening port: ', err.message);
    sequence(port).then((rt) => {
      console.log(" === ALL Process finished ===");
      process.exit(0);
    }, (e) => {
      console.error(e);
      process.exit(1);
    });
  });
});
