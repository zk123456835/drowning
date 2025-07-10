// pages/patrol/patrol.js
Page({
  data: {
    // 新增地图相关数据
    mapCtx: null,
    isLocationUpdating: false,
    // 模拟多个监控点
    markers: [
      { id: 1, longitude: 120.13026, latitude: 30.25961, name: "断桥水域" },
      { id: 2, longitude: 120.1428, latitude: 30.2485, name: "苏堤水域" },
      { id: 3, longitude: 120.1513, latitude: 30.2552, name: "三潭印月" }
    ]
  },

  onLoad() {
    this.refreshData();
    // 获取地图上下文
    this.mapCtx = wx.createMapContext('patrolMap');
    // 启动定位更新
    this.startLocationUpdate();
  },
  
  // 启动实时定位
  startLocationUpdate() {
    if (this.data.isLocationUpdating) return;
    
    this.setData({ isLocationUpdating: true });
    wx.startLocationUpdate({
      success: (res) => {
        console.log('定位启动成功', res);
        // 每10秒更新一次位置
        this.locationUpdateInterval = setInterval(() => {
          wx.getLocation({
            success: (locationRes) => {
              // 更新地图中心
              this.setData({
                longitude: locationRes.longitude,
                latitude: locationRes.latitude
              });
              // 更新标记点状态（模拟）
              this.refreshData();
            }
          });
        }, 10000);
      },
      fail: (err) => {
        console.error('定位启动失败', err);
        wx.showToast({ title: '定位失败，请检查权限', icon: 'error' });
      }
    });
  },
  // 刷新监控数据
  refreshData() {
    const now = new Date();
    const timeStr = `${now.getFullYear()}-${now.getMonth()+1}-${now.getDate()} ${now.getHours()}:${now.getMinutes()}:${now.getSeconds()}`;
    
    // 模拟随机落水事件（20%概率）
    const isDrowning = Math.random() > 0.8;
    
    this.setData({
      updateTime: timeStr,
      drowning: isDrowning,
      statusText: isDrowning ? "危险" : "安全",
      statusIcon: isDrowning ? "/images/warning.png" : "/images/safe.png",
      statusClass: isDrowning ? "warning" : "safe",
      markers: [{
        id: 1,
        longitude: 120.13026,
        latitude: 30.25961,
        iconPath: isDrowning ? '/images/warning.png' : '/images/safe.png',
        width: 40,
        height: 40
      }]
    });
    
    wx.showToast({
      title: '数据已更新',
      icon: 'success',
      duration: 1000
    });
  },

  // 返回首页
  goBack() {
    wx.navigateBack();
  }
});