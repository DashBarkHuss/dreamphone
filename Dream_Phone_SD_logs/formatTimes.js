fs = require("fs");
const date = Date.UTC(2021, 05, 06, 23, 26, 00, 00); //ex:jun 3 11:21. replace with your start time
machineOn = new Date(date);
machineOn.setTime(
  machineOn.getTime() + machineOn.getTimezoneOffset() * 60 * 1000
); //central time

getLogTime = (millisStr) => {
  const millis = +millisStr;
  const nd = new Date(machineOn);
  nd.setTime(nd.getTime() + millis);
  const toTime = `${nd.getHours() > 12 ? nd.getHours() - 12 : nd.getHours()}:${
    nd.getMinutes() < 10 ? "0" + nd.getMinutes() : nd.getMinutes()
  }:${nd.getSeconds() < 10 ? "0" + nd.getSeconds() : nd.getSeconds()}${
    nd.getHours() > 12 ? "p" : "a"
  }`;
  return toTime;
};
let logs = fs.readFileSync("./TEST.TXT", "utf8");
logs = logs.replace(/(?<!:[A-Za-z\d ]+)\d+/g, getLogTime); // (?<!: )\ ignore ": " because brightness is next
const fileName = `${
  machineOn.getMonth() + 1
}-${machineOn.getDate()}-${machineOn.getFullYear()}_${
  machineOn.getHours() > 12 ? "pm" : "am"
}.txt`;

fs.writeFileSync(fileName, logs);
console.log(logs);
