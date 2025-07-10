// pages/history/history.js
Page({
  data: {
    totalRescues: 23,
    successRate: 100,
    avgResponse: 3.2,
    historyList: [
      { time: "07-09 14:30", location: "试验水域", status: "成功" },
      { time: "07-09 11:20", location: "试验水域", status: "成功" },
      { time: "07-08 16:45", location: "试验水域", status: "成功" },
      { time: "07-07 09:15", location: "试验水域", status: "成功" },
      { time: "07-06 18:30", location: "试验水域", status: "成功" },
      { time: "07-05 15:10", location: "试验水域", status: "成功" },
      { time: "07-05 10:05", location: "试验水域", status: "成功" },
      { time: "07-5 08:20", location: "试验", status: "成功" }
    ]
  },

  goBack() {
    wx.navigateBack();
  }
});