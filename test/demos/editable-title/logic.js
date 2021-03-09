
export const title = ({title}) => title;

export const setTitle = (ctx, ev) => ev.target.value;

export const resetTitle = ({oldTitle}) => oldTitle;

export const titleIsValid = ({title}) => title.length > 0;