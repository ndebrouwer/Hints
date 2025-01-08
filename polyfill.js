if (typeof atob === 'undefined') {
    global.atob = (base64) => Buffer.from(base64, 'base64').toString('binary');
  }
  