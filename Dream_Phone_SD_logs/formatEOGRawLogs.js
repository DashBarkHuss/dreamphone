fs = require("fs");
let eog = fs.readFileSync("./RAW_EOG.TXT", "utf8");

const result = [...eog.matchAll(/(time|EOG) : [\d]+/g)].reduce((a, c, i) => {
  s = c[0].split(" : ");
  if (s[0] === "time") {
    const obj = {};
    obj[s[0]] = s[1];
    a.push(obj);
  } else {
    a[a.length - 1][s[0]] = s[1];
  }
  return a;
}, []);

fs.writeFileSync(`raw_eog_${Date.now()}.json`, JSON.stringify(result));
