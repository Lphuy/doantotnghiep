// Firebase configuration
var firebaseConfig = {
  apiKey: "AIzaSyD7e4ycqfitEBLi0nykchIa9A1WFBnhFO4",
  authDomain: "esp32-c5d33.firebaseapp.com",
  databaseURL: "https://esp32-c5d33-default-rtdb.asia-southeast1.firebasedatabase.app",
  projectId: "esp32-c5d33",
  storageBucket: "esp32-c5d33.appspot.com",
  messagingSenderId: "184153973143",
  appId: "1:184153973143:web:30f9ef8da5876899fbe4d2"
};
// Initialize Firebase
firebase.initializeApp(firebaseConfig);

// Reference to your entire Firebase database
var database = firebase.database();

// Reference to the "products" collection in your database
var productsRef = database.ref('products');

// Function to create a table row for a product
function createProductRow(product) {
  var row = document.createElement('tr');
  row.innerHTML = `
      <td>${product.code}</td>
      <td>${product.name}</td>
      <td>${product.quantityIn}</td>
      <td>${product.quantityOut}</td>
      <td>${product.status}</td>
  `;
  return row;
}

// Listen for changes to the "products" collection in your database
productsRef.on('value', function(snapshot) {
  var productTable = document.getElementById('productTable');
  // Clear existing rows (except the header)
  productTable.innerHTML = `
      <tr>
          <th>Mã Sản Phẩm</th>
          <th>Tên Sản Phẩm</th>
          <th>Số Lượng Nhập Vào</th>
          <th>Số Lượng Xuất Kho</th>
          <th>Tình Trạng Hàng</th>
      </tr>
  `;
  // Add a new row for each product in the database
  snapshot.forEach(function(childSnapshot) {
      var product = childSnapshot.val();
      var row = createProductRow(product);
      productTable.appendChild(row);
  });
});
