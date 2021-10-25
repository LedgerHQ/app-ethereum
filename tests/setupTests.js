import expect from 'expect'

expect.extend({
    toMatchSnapshot(received, original) {

      if(received.data.equals(original.data)){
        return {
          message: () => `snapshots are equal`,
          pass: true
        }
      } else {
        console.log("snapshots are not equal")
        return {
          message: () => `snapshots are not equal`,
          pass: false
        }
      }
    },
  });