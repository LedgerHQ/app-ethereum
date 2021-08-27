import expect from 'expect'

expect.extend({
    toMatchSnapshot(received, original) {

      if(received.data.equals(original.data)){
        return {
          message: () => `snapshots are equal`,
          pass: true
        }
      }

    //   console.log(received.data.toString('hex'))
    //   console.log(original.data.toString('hex'))

      return {
        message: () => `snapshots are not equal`,
        pass: false
      }
    },
  });